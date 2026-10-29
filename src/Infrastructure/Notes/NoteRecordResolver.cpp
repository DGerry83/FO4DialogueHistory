#include "NoteRecordResolver.h"

#include "Core/ILogger.h"
#include "Core/Note/NoteTextFormatter.h"

#include "RE/B/BGSKeyword.hpp"
#include "RE/B/BGSLocation.hpp"
#include "RE/C/Calendar.hpp"
#include "RE/O/OBJ_BOOK.hpp"
#include "RE/P/PlayerCharacter.hpp"
#include "RE/T/TESDescription.hpp"
#include "RE/T/TESFile.hpp"
#include "RE/T/TESForm.hpp"
#include "RE/T/TESFullName.hpp"
#include "RE/T/TESModel.hpp"
#include "RE/T/TESObjectBOOK.hpp"
#include "RE/T/TESObjectCELL.hpp"
#include "RE/T/TESObjectREFR.hpp"
#include "RE/T/TESWorldSpace.hpp"

#include <Windows.h>  // GetCurrentThreadId — deferred-task thread confirmation

#include <format>
#include <string>
#include <string_view>
#include <utility>

// M1 game-thread probes (see header). Everything here runs on the game
// thread via the F4SE task interface; engine calls are permitted here and
// forbidden in the event delivery path. Logging-only — no state survives
// between calls except one-shot diagnostic flags.

namespace
{
	constexpr std::size_t kPrefixLen = 72;       // bytes of candidate text per log line (UTF-8 safe prefix may split a sequence; diagnostic only)
	constexpr std::uint32_t kMaxKeywordsLogged = 6;

	struct TextStats
	{
		std::size_t length{ 0 };
		std::uint32_t aliasTokens{ 0 };
		std::uint32_t fontTags{ 0 };
		std::uint32_t imgTags{ 0 };
		bool        pagebreak{ false };
	};

	std::uint32_t CountOccurrences(std::string_view a_text, std::string_view a_needle) noexcept
	{
		std::uint32_t count = 0;
		std::size_t   pos = 0;
		while ((pos = a_text.find(a_needle, pos)) != std::string_view::npos) {
			++count;
			pos += a_needle.size();
		}
		return count;
	}

	TextStats AnalyzeText(std::string_view a_text)
	{
		TextStats stats;
		stats.length = a_text.size();
		stats.aliasTokens = CountOccurrences(a_text, "<Alias");
		stats.fontTags = CountOccurrences(a_text, "<font");
		stats.imgTags = CountOccurrences(a_text, "<img");
		stats.pagebreak = a_text.find("[pagebreak]") != std::string_view::npos;
		return stats;
	}

	// Log-friendly prefix: control characters escaped, length-bounded. The
	// 72-byte cut can split a UTF-8 sequence — acceptable for a diagnostic
	// prefix; lengths are reported exactly so nothing is hidden.
	std::string SanitizePrefix(std::string_view a_text)
	{
		std::string out;
		out.reserve(std::min(a_text.size(), kPrefixLen) + 8);
		for (char c : a_text) {
			if (out.size() >= kPrefixLen) {
				break;
			}
			switch (c) {
			case '\r': out += "\\r"; break;
			case '\n': out += "\\n"; break;
			case '\t': out += "\\t"; break;
			default:   out += c; break;
			}
		}
		return out;
	}

	// R1 full-body evidence for the G2 visible-page comparison: every control
	// character escaped, and backslash escaped first so the escapes stay
	// unambiguous — each route stays one parseable log line. Unbounded length
	// is accepted only on this path: it runs on the game thread, only for
	// actual book-read events (user-action rate, never per-frame), and only
	// when VerboseCapture=1 has already admitted the observation.
	std::string EscapeForLog(std::string_view a_text)
	{
		std::string out;
		out.reserve(a_text.size() * 2);
		for (char c : a_text) {
			switch (c) {
			case '\\': out += "\\\\"; break;
			case '"': out += "\\\""; break;
			case '\r': out += "\\r"; break;
			case '\n': out += "\\n"; break;
			case '\t': out += "\\t"; break;
			default:   out += c; break;
			}
		}
		return out;
	}

	std::string FormatStats(const TextStats& a_stats)
	{
		return std::format("len={} aliases={} pagebreak={} font={} img={}",
			a_stats.length, a_stats.aliasTokens, a_stats.pagebreak ? 1 : 0, a_stats.fontTags, a_stats.imgTags);
	}

	[[nodiscard]] std::string Safe(const char* a_cstr)
	{
		return (a_cstr && a_cstr[0] != '\0') ? std::string(a_cstr) : std::string{};
	}

	std::string DecodeBookFlags(std::uint8_t a_flags)
	{
		std::string out;
		const auto  append = [&out](std::string_view a_name) {
			if (!out.empty()) {
				out += '|';
			}
			out += a_name;
		};
		if (a_flags & static_cast<std::uint8_t>(RE::OBJ_BOOK::Flags::kAdvancesActorValue)) append("AdvancesActorValue");
		if (a_flags & static_cast<std::uint8_t>(RE::OBJ_BOOK::Flags::kCannotTake)) append("CannotTake");
		if (a_flags & static_cast<std::uint8_t>(RE::OBJ_BOOK::Flags::kAddSpell)) append("AddSpell");
		if (a_flags & static_cast<std::uint8_t>(RE::OBJ_BOOK::Flags::kHasBeenRead)) append("HasBeenRead");
		if (a_flags & static_cast<std::uint8_t>(RE::OBJ_BOOK::Flags::kAddPerk)) append("AddPerk");
		if (out.empty()) {
			out = "none";
		}
		return out;
	}

	// NEXT_PROBE_CONTRACT magazine probe: teaching flags (kAddPerk/kAddSpell/
	// kAdvancesActorValue — never CantBeTaken/HasBeenRead) plus the
	// PerkMagKeyword runtime form ID scanned over the FULL keyword array
	// (the compact identity log still shows at most six). Text state is
	// never an input: an empty body is not a classification signal.
	constexpr std::uint32_t kPerkMagKeywordID = 0x001D4A70;

	std::string ClassifyBookVerdict(const RE::TESObjectBOOK& a_book)
	{
		const auto flags = a_book.data.flags.underlying();
		const bool teaches =
			(flags & static_cast<std::uint8_t>(RE::OBJ_BOOK::Flags::kAddPerk)) != 0 ||
			(flags & static_cast<std::uint8_t>(RE::OBJ_BOOK::Flags::kAddSpell)) != 0 ||
			(flags & static_cast<std::uint8_t>(RE::OBJ_BOOK::Flags::kAdvancesActorValue)) != 0;

		bool perkMag = false;
		const std::uint32_t keywordCount = a_book.numKeywords;
		for (std::uint32_t i = 0; i < keywordCount; ++i) {
			const RE::BGSKeyword* keyword = a_book.keywords[i];
			if (keyword && keyword->formID == kPerkMagKeywordID) {
				perkMag = true;
				break;
			}
		}

		if (teaches && perkMag) return "reject:both";
		if (teaches) return "reject:teaching";
		if (perkMag) return "reject:perkmag";
		return "accept";
	}

	std::string DescribeBookIdentity(const RE::TESObjectBOOK& a_book)
	{
		const RE::TESForm& form = a_book;

		std::string plugin = "<no file>";
		if (const RE::TESFile* file = form.GetFile()) {
			plugin = file->filename.data();
		}

		const auto* model = static_cast<const RE::TESModel*>(&a_book)->GetModel();

		std::string keywords = "[";
		const std::uint32_t keywordCount = a_book.numKeywords;
		const std::uint32_t logged = std::min(keywordCount, kMaxKeywordsLogged);
		for (std::uint32_t i = 0; i < logged; ++i) {
			const RE::BGSKeyword* keyword = a_book.keywords[i];
			keywords += std::format("{}{}", i == 0 ? "" : ",", keyword ? Safe(keyword->GetFormEditorID()) : std::string("<null>"));
		}
		if (keywordCount > logged) {
			keywords += std::format(",... (+{} more)", keywordCount - logged);
		}
		keywords += ']';

		const auto flags = a_book.data.flags.underlying();
		return std::format("editorID='{}' name='{}' plugin={} localId=0x{:06X} flags=[{}] verdict={} keywords={} model='{}'",
			Safe(form.GetFormEditorID()),
			Safe(a_book.GetFullName()),
			plugin,
			form.GetLocalFormID(),
			DecodeBookFlags(flags),
			ClassifyBookVerdict(a_book),
			keywords,
			Safe(model));
	}

	std::string FormatGameTime()
	{
		const RE::Calendar* calendar = RE::Calendar::GetSingleton();
		if (!calendar) {
			return "<calendar unavailable>";
		}
		const auto  year = static_cast<int>(calendar->GetYear());
		const auto  month = static_cast<unsigned>(calendar->GetMonth());
		const auto  day = static_cast<unsigned>(calendar->GetDay());
		const float hour = calendar->GetHour().count();
		const float minute = calendar->GetMinute().count();
		return std::format("{:04d}-{:02d}-{:02d} {:.2f}:{:05.2f} (raw hour={:.2f} minute={:.2f})",
			year, month, day, hour, minute, hour, minute);
	}

	std::pair<std::string, const char*> FormatPlayerLocation(F4DH::Core::ILogger& a_log)
	{
		const RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			static bool warned = false;
			if (!warned) {
				warned = true;
				a_log.Warn("note probe: PlayerCharacter singleton unavailable — location context will read <player unavailable>");
			}
			return { "<player unavailable>", "none" };
		}
		if (const RE::BGSLocation* location = player->GetCurrentLocation()) {
			if (const char* name = location->GetFullName(); name && name[0] != '\0') {
				return { name, "location" };
			}
		}
		if (const RE::TESObjectCELL* cell = player->GetParentCell()) {
			if (const char* name = cell->GetFullName(); name && name[0] != '\0') {
				return { name, "cell" };
			}
			if (const RE::TESWorldSpace* worldspace = cell->GetWorldSpace()) {
				if (const char* name = worldspace->GetFullName(); name && name[0] != '\0') {
					return { name, "worldspace" };
				}
			}
			if (const char* editorID = cell->GetFormEditorID(); editorID && editorID[0] != '\0') {
				return { std::format("<unnamed cell {}>", editorID), "cell-editorid" };
			}
		}
		return { "Unknown location", "unknown" };
	}

	// Record-once classifier notice (NEXT_PROBE_CONTRACT): teaching flags plus
	// the PerkMagKeyword runtime marker are the active magazine classifier;
	// VMAD/native-script metadata is not required. Replaces the earlier
	// script-metadata BLOCKED warning once the keyword classifier landed.
	void LogClassifierActiveOnce(F4DH::Core::ILogger& a_log)
	{
		static bool logged = false;
		if (!logged) {
			logged = true;
			a_log.Info("note probe: magazine classifier active — teaching flags (kAddPerk|kAddSpell|kAdvancesActorValue) + PerkMagKeyword (0x001D4A70) over the full keyword array; VMAD/native-script metadata not required");
		}
	}

	// Confirms once that the deferred path executed; the per-line [Thread:]
	// stamps are the authoritative record of WHICH thread ran each deferred
	// observation — install ran on the kGameDataReady messaging thread, which
	// is a different context and not the comparison target.
	void LogFirstTaskThreadOnce(F4DH::Core::ILogger& a_log)
	{
		static bool logged = false;
		if (!logged) {
			logged = true;
			a_log.Info(std::format("note probe: first deferred observation executing on thread {} (per-line [Thread:] stamps record the deferred thread; do not compare with the install-time thread)",
				::GetCurrentThreadId()));
		}
	}

	const RE::TESObjectREFR* ResolveReference(std::uint32_t a_formID)
	{
		if (a_formID == 0) return nullptr;
		const RE::TESForm* form = RE::TESForm::FindFormByID(a_formID);
		if (!form && (a_formID & 0xFF000000u) == 0xFF000000u) {
			form = RE::TESForm::FindFormByNumericID(a_formID);
		}
		return form ? form->As<RE::TESObjectREFR>() : nullptr;
	}

	F4DH::Core::NoteSourceTraits TraitsFor(const RE::TESObjectBOOK& a_book)
	{
		F4DH::Core::NoteSourceTraits traits;
		const auto flags = a_book.data.flags.underlying();
		traits.teaches =
			(flags & static_cast<std::uint8_t>(RE::OBJ_BOOK::Flags::kAddPerk)) != 0 ||
			(flags & static_cast<std::uint8_t>(RE::OBJ_BOOK::Flags::kAddSpell)) != 0 ||
			(flags & static_cast<std::uint8_t>(RE::OBJ_BOOK::Flags::kAdvancesActorValue)) != 0;
		for (std::uint32_t i = 0; i < a_book.numKeywords; ++i) {
			const auto* keyword = a_book.keywords[i];
			if (keyword && keyword->formID == kPerkMagKeywordID) {
				traits.hasPerkMagKeyword = true;
				break;
			}
		}
		return traits;
	}

	F4DH::Core::EncounterMetadata CurrentEncounter(F4DH::Core::ILogger& a_log, F4DH::Core::EncounterKind a_kind)
	{
		F4DH::Core::EncounterMetadata encounter;
		encounter.kind = a_kind;
		if (const auto* calendar = RE::Calendar::GetSingleton()) {
			encounter.timestamp.year = static_cast<std::int32_t>(calendar->GetYear());
			encounter.timestamp.month = static_cast<std::uint8_t>(static_cast<unsigned>(calendar->GetMonth()));
			encounter.timestamp.day = static_cast<std::uint8_t>(static_cast<unsigned>(calendar->GetDay()));
			encounter.timestamp.hour = static_cast<std::uint8_t>(calendar->GetHour().count());
			encounter.timestamp.minute = static_cast<std::uint8_t>(calendar->GetMinute().count());
		}
		encounter.location = FormatPlayerLocation(a_log).first;
		return encounter;
	}

	F4DH::Core::NoteIdentity IdentityFor(const RE::TESObjectBOOK& a_book, const RE::TESObjectREFR* a_refr, std::uint16_t a_uniqueID, bool a_dynamic)
	{
		F4DH::Core::NoteIdentity identity;
		identity.kind = a_dynamic ?
			(a_refr ? F4DH::Core::NoteIdentityKind::DynamicReference : F4DH::Core::NoteIdentityKind::DynamicSynthetic) :
			F4DH::Core::NoteIdentityKind::StaticBase;
		identity.baseFormID = a_book.formID;
		identity.localBaseFormID = a_book.GetLocalFormID();
		identity.sourceRefFormID = a_refr ? a_refr->formID : 0;
		identity.runtimeUniqueID = a_uniqueID;
		if (const auto* file = a_book.GetFile()) identity.pluginName = file->filename.data();
		return identity;
	}

	std::string TitleFor(const RE::TESObjectBOOK& a_book, const RE::TESObjectREFR* a_refr)
	{
		if (a_refr) {
			const auto display = RE::TESFullName::GetFormFullOrDisplayName(a_refr);
			if (display && display->data() && display->data()[0] != '\0') return display->data();
		}
		return Safe(a_book.GetFullName());
	}

	bool HasRawAlias(const RE::TESObjectBOOK& a_book)
	{
		RE::BSString text;
		a_book.GetDescription(text, nullptr);
		return text.c_str() && F4DH::Core::NoteTextFormatter::ContainsUnresolvedAlias(text.c_str());
	}
}

namespace F4DH::Infrastructure::Notes
{
	void NoteRecordResolver::LogReadObservation(const ReadObservation& a_obs, Core::ILogger& a_log)
	{
		LogFirstTaskThreadOnce(a_log);
		LogClassifierActiveOnce(a_log);

		a_log.Info(std::format("note probe [seq {}]: READ base=0x{:08X} ref={} unique={} fromThread={}",
			a_obs.seq,
			a_obs.baseFormID,
			a_obs.bookRefFormID != 0 ? std::format("0x{:08X}", a_obs.bookRefFormID) : std::string("null"),
			a_obs.uniqueID,
			a_obs.fromThreadId));

		// --- Identity resolution (OG runtime-evidence fix, RUNTIME_RESULTS.md
		// seq931-942): the event baseFormID is unreliable — inventory reads
		// carried the player's own ID (0x14, type 65) and dynamic/world reads
		// carried 0. Both candidates are resolved and type-checked here; the
		// reference base names the actually-displayed instance and is
		// preferred, with the event base as fallback. Identities are never
		// merged silently: every outcome is logged explicitly.
		const RE::TESForm* eventBaseForm = nullptr;
		if (a_obs.baseFormID != 0) {
			eventBaseForm = RE::TESForm::FindFormByID(a_obs.baseFormID);
		}
		const auto* eventBook = (eventBaseForm && eventBaseForm->Is(RE::FormType::kBook)) ?
			static_cast<const RE::TESObjectBOOK*>(eventBaseForm) :
			nullptr;

		// 0xFF00xxxx references are runtime-created; the ordinary map lookup
		// can miss them, so fall back to the numeric form map (the
		// QuestResolver-proven path for runtime IDs, OG-verified globals).
		const RE::TESObjectREFR* refr = nullptr;
		const RE::TESForm*         refBaseForm = nullptr;
		const char*                refResolve = "none";
		if (a_obs.bookRefFormID != 0) {
			refResolve = "unresolved";
			const RE::TESForm* refForm = RE::TESForm::FindFormByID(a_obs.bookRefFormID);
			if (refForm) {
				refResolve = "map";
			} else if ((a_obs.bookRefFormID & 0xFF000000u) == 0xFF000000u) {
				refForm = RE::TESForm::FindFormByNumericID(a_obs.bookRefFormID);
				refResolve = refForm ? "numeric" : "unresolved";
			}
			refr = refForm ? refForm->As<RE::TESObjectREFR>() : nullptr;
			if (refForm && !refr) {
				refResolve = "notaref";
			}
			if (refr) {
				refBaseForm = refr->GetBaseObject();
			}
		}
		const auto* refBook = (refBaseForm && refBaseForm->Is(RE::FormType::kBook)) ?
			static_cast<const RE::TESObjectBOOK*>(refBaseForm) :
			nullptr;

		const RE::TESObjectBOOK* book = refBook ? refBook : eventBook;
		const char*              source = refBook ? "ref" : (eventBook ? "event" : "none");
		const bool               mismatch = eventBook && refBook && eventBook != refBook;

		const auto describe = [](std::uint32_t a_id, const RE::TESForm* a_form, const RE::TESObjectBOOK* a_book) {
			if (a_id == 0) {
				return std::string("none");
			}
			if (!a_form) {
				return std::format("0x{:08X} unresolved", a_id);
			}
			if (!a_book) {
				return std::format("0x{:08X} notabook:{}", a_id, static_cast<std::uint32_t>(a_form->GetFormType()));
			}
			return std::format("0x{:08X} book", a_id);
		};
		a_log.Info(std::format("note probe [seq {}]: identity eventBase={} refBase={} selected=0x{:08X} source={} mismatch={} refResolve={}",
			a_obs.seq,
			describe(a_obs.baseFormID, eventBaseForm, eventBook),
			describe(refBaseForm ? refBaseForm->formID : 0u, refBaseForm, refBook),
			book ? book->formID : 0u,
			source,
			mismatch ? 1 : 0,
			refResolve));

		if (!book) {
			a_log.Warn(std::format("note probe [seq {}]: neither event base nor reference base resolved to a book — no body probes for this read",
				a_obs.seq));
			return;
		}
		if (mismatch) {
			// Both resolved to books but disagree: log the non-selected
			// identity too so the divergence is preserved in evidence.
			a_log.Info(std::format("note probe [seq {}]: base (unselected eventBase): {}", a_obs.seq, DescribeBookIdentity(*eventBook)));
		}
		a_log.Info(std::format("note probe [seq {}]: base: {}", a_obs.seq, DescribeBookIdentity(*book)));

		// D8: probe both completed description routes; the checklist compares
		// these candidates against the visible page. Two further routes are
		// permanently disabled on OG runtime evidence (session
		// 2026-10-28_Bug_NoteReadFreeze): the static form-description route
		// froze the process on an inventory note read, and the reference-parent
		// route added in the same investigation remains unverified. Each skip
		// is logged explicitly in the body block below.
		a_log.Info(std::format("note probe [seq {}]: text-stage inherited begin", a_obs.seq));
		RE::BSString inheritedText;
		book->GetDescription(inheritedText, nullptr);
		a_log.Info(std::format("note probe [seq {}]: text-stage inherited end", a_obs.seq));
		const std::string_view inheritedView = inheritedText.c_str() ? std::string_view(inheritedText.c_str()) : std::string_view{};
		const TextStats inheritedStats = AnalyzeText(inheritedView);

		a_log.Info(std::format("note probe [seq {}]: text-stage itemCard begin", a_obs.seq));
		RE::BSString itemCardText;
		book->itemCardDescription.GetDescription(itemCardText, book);
		a_log.Info(std::format("note probe [seq {}]: text-stage itemCard end", a_obs.seq));
		const std::string_view itemCardView = itemCardText.c_str() ? std::string_view(itemCardText.c_str()) : std::string_view{};
		const TextStats itemCardStats = AnalyzeText(itemCardView);

		a_log.Info(std::format("note probe [seq {}]: text inherited({}) prefix=\"{}\" | itemCard({}) prefix=\"{}\"",
			a_obs.seq,
			FormatStats(inheritedStats), SanitizePrefix(inheritedView),
			FormatStats(itemCardStats), SanitizePrefix(itemCardView)));

		// R1: full candidate bodies — the exact visible-page comparison input
		// for G2. One self-contained escaped line per route; empty vs missing
		// stays distinguishable (len=0 "" vs absent).
		a_log.Info(std::format("note probe [seq {}]: body inherited len={} \"{}\"",
			a_obs.seq, inheritedView.size(), EscapeForLog(inheritedView)));
		a_log.Info(std::format("note probe [seq {}]: body itemCard len={} \"{}\"",
			a_obs.seq, itemCardView.size(), EscapeForLog(itemCardView)));
		// Neither disabled route may be re-enabled without a new safety plan.
		a_log.Info(std::format("note probe [seq {}]: body formDesc skipped (unsafe static route removed)", a_obs.seq));
		a_log.Info(std::format("note probe [seq {}]: body inheritedRef skipped (reference-parent route unverified)", a_obs.seq));

		const auto [location, locationVia] = FormatPlayerLocation(a_log);
		a_log.Info(std::format("note probe [seq {}]: context gameTime={} location='{}' via={}",
			a_obs.seq, FormatGameTime(), location, locationVia));

		if (refr) {
			const auto* base = refr->GetBaseObject();
			const auto  refDisplay = RE::TESFullName::GetFormFullOrDisplayName(refr);
			a_log.Info(std::format("note probe [seq {}]: bookRef editorID='{}' displayName='{}' base=0x{:08X} baseMatchesEvent={}",
				a_obs.seq,
				Safe(refr->GetFormEditorID()),
				refDisplay ? Safe(refDisplay->data()) : std::string("<none>"),
				base ? base->formID : 0u,
				base && base->formID == a_obs.baseFormID ? 1 : 0));
		} else if (a_obs.bookRefFormID != 0) {
			a_log.Info(std::format("note probe [seq {}]: bookRef 0x{:08X} did not resolve to a reference (refResolve={})",
				a_obs.seq, a_obs.bookRefFormID, refResolve));
		}
	}

	void NoteRecordResolver::LogTransferObservation(const TransferObservation& a_obs, Core::ILogger& a_log)
	{
		LogFirstTaskThreadOnce(a_log);

		const RE::TESForm* form = RE::TESForm::FindFormByID(a_obs.baseItemFormID);
		if (!form || !form->Is(RE::FormType::kBook)) {
			return;  // non-book transfer: silent drop keeps the probe log readable
		}

		const RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
		const bool playerBound = player &&
			(a_obs.newContainerFormID == player->formID || a_obs.oldContainerFormID == player->formID);

		a_log.Info(std::format("note probe [seq {}]: TRANSFER base=0x{:08X} count={} old=0x{:08X} new=0x{:08X} ref={} unique={} playerBound={} fromThread={}",
			a_obs.seq,
			a_obs.baseItemFormID,
			a_obs.itemCount,
			a_obs.oldContainerFormID,
			a_obs.newContainerFormID,
			a_obs.itemRefFormID != 0 ? std::format("0x{:08X}", a_obs.itemRefFormID) : std::string("null"),
			a_obs.uniqueID,
			playerBound ? 1 : 0,
			a_obs.fromThreadId));

		const auto* book = static_cast<const RE::TESObjectBOOK*>(form);
		a_log.Info(std::format("note probe [seq {}]: base: {}", a_obs.seq, DescribeBookIdentity(*book)));

		if (playerBound) {
			const auto [location, locationVia] = FormatPlayerLocation(a_log);
			a_log.Info(std::format("note probe [seq {}]: context gameTime={} location='{}' via={} (first-encounter metadata probe)",
				a_obs.seq, FormatGameTime(), location, locationVia));
		}
	}

	void NoteRecordResolver::LogPreparedBookText(PreparedTextObservation a_obs, Core::ILogger& a_log)
	{
		LogFirstTaskThreadOnce(a_log);
		LogClassifierActiveOnce(a_log);

		a_log.Info(std::format("note probe [bookText {}]: PREPARED book=0x{:08X} ref=0x{:08X} len={} copied={} truncated={} textState={} fromThread={}",
			a_obs.seq,
			a_obs.bookFormID,
			a_obs.refFormID,
			a_obs.finalLength,
			a_obs.body.size(),
			a_obs.truncated ? 1 : 0,
			a_obs.body.empty() ? "empty" : "present",
			a_obs.fromThreadId));

		// Identity: prefer the target-book slot, fall back to the reference
		// base — the same no-silent-merge discipline as the read probe.
		const RE::TESForm* form = a_obs.bookFormID != 0 ? RE::TESForm::FindFormByID(a_obs.bookFormID) : nullptr;
		const RE::TESObjectBOOK* book = (form && form->Is(RE::FormType::kBook)) ? static_cast<const RE::TESObjectBOOK*>(form) : nullptr;
		const char* source = book ? "slot" : "none";

		const RE::TESObjectREFR* refr = nullptr;
		if (!book && a_obs.refFormID != 0) {
			if (const RE::TESForm* refForm = RE::TESForm::FindFormByID(a_obs.refFormID)) {
				refr = refForm->As<RE::TESObjectREFR>();
			}
			if (!refr && (a_obs.refFormID & 0xFF000000u) == 0xFF000000u) {
				if (const RE::TESForm* refForm = RE::TESForm::FindFormByNumericID(a_obs.refFormID)) {
					refr = refForm->As<RE::TESObjectREFR>();
				}
			}
			if (refr) {
				if (const RE::TESForm* base = refr->GetBaseObject(); base && base->Is(RE::FormType::kBook)) {
					book = static_cast<const RE::TESObjectBOOK*>(base);
					source = "refBase";
				}
			}
		} else if (book && a_obs.refFormID != 0) {
			if (const RE::TESForm* refForm = RE::TESForm::FindFormByID(a_obs.refFormID)) {
				refr = refForm->As<RE::TESObjectREFR>();
			}
			if (!refr && (a_obs.refFormID & 0xFF000000u) == 0xFF000000u) {
				if (const RE::TESForm* refForm = RE::TESForm::FindFormByNumericID(a_obs.refFormID)) {
					refr = refForm->As<RE::TESObjectREFR>();
				}
			}
		}

		a_log.Info(std::format("note probe [bookText {}]: identity selected={} source={}",
			a_obs.seq, book ? std::format("0x{:08X}", book->formID) : std::string("<none>"), source));

		if (book) {
			a_log.Info(std::format("note probe [bookText {}]: base: {}", a_obs.seq, DescribeBookIdentity(*book)));
		} else {
			a_log.Warn(std::format("note probe [bookText {}]: neither slot book nor reference base resolved to a book — identity unproven",
				a_obs.seq));
		}

		if (refr) {
			const auto refDisplay = RE::TESFullName::GetFormFullOrDisplayName(refr);
			a_log.Info(std::format("note probe [bookText {}]: bookRef editorID='{}' displayName='{}'",
				a_obs.seq, Safe(refr->GetFormEditorID()), refDisplay ? Safe(refDisplay->data()) : std::string("<none>")));
		}

		a_log.Info(std::format("note probe [bookText {}]: body prepared len={} \"{}\"",
			a_obs.seq, a_obs.body.size(), EscapeForLog(a_obs.body)));

		const auto [location, locationVia] = FormatPlayerLocation(a_log);
		a_log.Info(std::format("note probe [bookText {}]: context gameTime={} location='{}' via={}",
			a_obs.seq, FormatGameTime(), location, locationVia));
	}

	std::optional<Application::NotePickupEvent> NoteRecordResolver::ResolveTransferObservation(const TransferObservation& a_obs, Core::ILogger& a_log)
	{
		const auto* form = RE::TESForm::FindFormByID(a_obs.baseItemFormID);
		if (!form || !form->Is(RE::FormType::kBook)) return std::nullopt;
		const auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || a_obs.newContainerFormID != player->formID) return std::nullopt;

		const auto* book = static_cast<const RE::TESObjectBOOK*>(form);
		const auto* refr = ResolveReference(a_obs.itemRefFormID);
		Application::NotePickupEvent event;
		event.identity = IdentityFor(*book, refr, a_obs.uniqueID, HasRawAlias(*book));
		event.title = TitleFor(*book, refr);
		event.traits = TraitsFor(*book);
		event.encounter = CurrentEncounter(a_log, Core::EncounterKind::Pickup);
		return event;
	}

	std::optional<Application::NoteReadEvent> NoteRecordResolver::ResolvePreparedText(const PreparedTextObservation& a_obs, Core::ILogger& a_log)
	{
		const RE::TESForm* form = a_obs.bookFormID != 0 ? RE::TESForm::FindFormByID(a_obs.bookFormID) : nullptr;
		const auto* book = (form && form->Is(RE::FormType::kBook)) ? static_cast<const RE::TESObjectBOOK*>(form) : nullptr;
		const auto* refr = ResolveReference(a_obs.refFormID);
		if (!book && refr) {
			const auto* base = refr->GetBaseObject();
			if (base && base->Is(RE::FormType::kBook)) book = static_cast<const RE::TESObjectBOOK*>(base);
		}
		if (!book) return std::nullopt;

		const bool dynamic = HasRawAlias(*book);
		Application::NoteReadEvent event;
		event.identity = IdentityFor(*book, refr, 0, dynamic);
		event.title = TitleFor(*book, refr);
		event.traits = TraitsFor(*book);
		event.encounter = CurrentEncounter(a_log, Core::EncounterKind::Read);

		RE::BSString inherited;
		book->GetDescription(inherited, nullptr);
		RE::BSString itemCard;
		book->itemCardDescription.GetDescription(itemCard, book);
		const bool noText = (!inherited.c_str() || inherited.c_str()[0] == '\0') &&
			(!itemCard.c_str() || itemCard.c_str()[0] == '\0') && a_obs.body.empty();
		if (noText) {
			const auto* model = static_cast<const RE::TESModel*>(book)->GetModel();
			event.traits.textureOnly = model && std::string_view(model) != "Props\\Note_LowPoly.nif";
			event.bodyMode = Core::NoteBodyMode::Empty;
			return event;
		}
		if (Core::NoteTextFormatter::ContainsUnresolvedAlias(a_obs.body)) {
			event.bodyMode = Core::NoteBodyMode::Unavailable;
			return event;
		}
		event.plainBody = Core::NoteTextFormatter::ToPlainText(a_obs.body);
		event.bodyMode = dynamic ? Core::NoteBodyMode::CachedResolved :
			(event.plainBody.empty() ? Core::NoteBodyMode::Empty : Core::NoteBodyMode::ResolveStatic);
		return event;
	}

	Core::NoteBodyResult NoteRecordResolver::BodySource::ResolveBody(const Core::NoteRecord& record)
	{
		Core::NoteBodyResult result;
		result.entryId = record.identity.entryId;
		result.title = record.title;
		result.pluginName = record.identity.pluginName;
		result.firstEncounter = record.firstEncounter;
		if (record.bodyMode == Core::NoteBodyMode::CachedResolved) {
			result.status = Core::NoteBodyResult::Status::Ready;
			result.body = record.cachedBody;
			return result;
		}
		if (record.bodyMode == Core::NoteBodyMode::Empty) {
			result.status = Core::NoteBodyResult::Status::Empty;
			return result;
		}
		if (record.bodyMode == Core::NoteBodyMode::Unavailable) {
			result.status = Core::NoteBodyResult::Status::Unavailable;
			return result;
		}

		const auto* form = RE::TESForm::FindFormByID(record.identity.baseFormID);
		if (!form || !form->Is(RE::FormType::kBook)) {
			result.status = Core::NoteBodyResult::Status::MissingPlugin;
			return result;
		}
		const auto* book = static_cast<const RE::TESObjectBOOK*>(form);
		RE::BSString text;
		book->GetDescription(text, nullptr);
		if (text.c_str() && Core::NoteTextFormatter::ContainsUnresolvedAlias(text.c_str())) {
			result.status = Core::NoteBodyResult::Status::Unavailable;
			return result;
		}
		result.body = Core::NoteTextFormatter::ToPlainText(text.c_str() ? text.c_str() : "");
		result.status = result.body.empty() ? Core::NoteBodyResult::Status::Empty : Core::NoteBodyResult::Status::Ready;
		return result;
	}
}
