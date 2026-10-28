#include "NoteRecordResolver.h"

#include "Core/ILogger.h"

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
		return std::format("editorID='{}' name='{}' plugin={} localId=0x{:06X} flags=[{}] keywords={} model='{}'",
			Safe(form.GetFormEditorID()),
			Safe(a_book.GetFullName()),
			plugin,
			form.GetLocalFormID(),
			DecodeBookFlags(flags),
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

	// D1 record-once: script metadata (e.g. CA_SkillMagazineScript) has no
	// accessor in the AV fork headers — this is the exact blocker for the G3
	// magazine-marker leg, logged in-band so every probe log carries it.
	void LogScriptMetadataBlockerOnce(F4DH::Core::ILogger& a_log)
	{
		static bool logged = false;
		if (!logged) {
			logged = true;
			a_log.Warn("note probe: script-metadata probe BLOCKED — AV fork exposes no VMAD/native-script accessor on TESForm/TESObjectBOOK (searched: BGSVMAD/VirtualMachineAdapter/GetVirtualMachineAdapter); magazine script markers (CA_SkillMagazineScript) unreachable natively — see session PROGRESS_LOG D1");
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
}

namespace F4DH::Infrastructure::Notes
{
	void NoteRecordResolver::LogReadObservation(const ReadObservation& a_obs, Core::ILogger& a_log)
	{
		LogFirstTaskThreadOnce(a_log);
		LogScriptMetadataBlockerOnce(a_log);

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

		// D8: probe both description routes plus the static one; the checklist
		// compares these candidates against the visible page. Runtime evidence
		// (seq931-942) adds a fourth candidate when the reference exists:
		// inherited description with the REFERENCE as parent — the route most
		// likely to carry stored/alias-resolved instance text.
		RE::BSString inheritedText;
		book->GetDescription(inheritedText, nullptr);
		const std::string_view inheritedView = inheritedText.c_str() ? std::string_view(inheritedText.c_str()) : std::string_view{};
		const TextStats inheritedStats = AnalyzeText(inheritedView);

		RE::BSString itemCardText;
		book->itemCardDescription.GetDescription(itemCardText, book);
		const std::string_view itemCardView = itemCardText.c_str() ? std::string_view(itemCardText.c_str()) : std::string_view{};
		const TextStats itemCardStats = AnalyzeText(itemCardView);

		const auto formDescription = RE::TESDescription::GetFormDescription(book);
		const std::string_view formDescView = formDescription ? std::string_view(formDescription->c_str()) : std::string_view{};
		const TextStats formDescStats = AnalyzeText(formDescView);

		const bool             haveRefRoute = refr != nullptr;
		RE::BSString           inheritedRefText;
		std::string_view       inheritedRefView;
		TextStats              inheritedRefStats{};
		if (haveRefRoute) {
			book->GetDescription(inheritedRefText, refr);
			inheritedRefView = inheritedRefText.c_str() ? std::string_view(inheritedRefText.c_str()) : std::string_view{};
			inheritedRefStats = AnalyzeText(inheritedRefView);
		}

		a_log.Info(std::format("note probe [seq {}]: text inherited({}) prefix=\"{}\" | itemCard({}) prefix=\"{}\" | formDesc present={} ({}) prefix=\"{}\" | inheritedRef={} ({}) prefix=\"{}\"",
			a_obs.seq,
			FormatStats(inheritedStats), SanitizePrefix(inheritedView),
			FormatStats(itemCardStats), SanitizePrefix(itemCardView),
			formDescription ? 1 : 0,
			FormatStats(formDescStats), SanitizePrefix(formDescView),
			haveRefRoute ? 1 : 0,
			FormatStats(inheritedRefStats), SanitizePrefix(inheritedRefView)));

		// R1: full candidate bodies — the exact visible-page comparison input
		// for G2. One self-contained escaped line per route; empty vs missing
		// stays distinguishable (len=0 "" vs absent).
		a_log.Info(std::format("note probe [seq {}]: body inherited len={} \"{}\"",
			a_obs.seq, inheritedView.size(), EscapeForLog(inheritedView)));
		a_log.Info(std::format("note probe [seq {}]: body itemCard len={} \"{}\"",
			a_obs.seq, itemCardView.size(), EscapeForLog(itemCardView)));
		if (formDescription) {
			a_log.Info(std::format("note probe [seq {}]: body formDesc len={} \"{}\"",
				a_obs.seq, formDescView.size(), EscapeForLog(formDescView)));
		} else {
			a_log.Info(std::format("note probe [seq {}]: body formDesc absent", a_obs.seq));
		}
		if (haveRefRoute) {
			a_log.Info(std::format("note probe [seq {}]: body inheritedRef len={} \"{}\"",
				a_obs.seq, inheritedRefView.size(), EscapeForLog(inheritedRefView)));
		} else {
			a_log.Info(std::format("note probe [seq {}]: body inheritedRef skipped (no bookRef)", a_obs.seq));
		}

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
}
