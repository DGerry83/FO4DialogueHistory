#include "SubtitleHook.h"

#include "Application/Capture/ISubtitleSink.h"
#include "Core/Dialogue/SubtitleEvent.h"

#include "REL/REL.hpp"

#include "RE/A/Actor.hpp"
#include "RE/B/BGSScene.hpp"
#include "RE/F/FormType.hpp"
#include "RE/M/MenuTopicManager.hpp"
#include "RE/T/TESNPC.hpp"
#include "RE/T/TESObjectREFR.hpp"
#include "RE/T/TESTopicInfo.hpp"

#include "REX/Log.hpp"

#include <cstring>

// Prologue detour on SubtitleManager::ShowSubtitle, the pattern proven by
// powerof3/FloatingSubtitlesF4 and northaxosky's 1.10.163 port: steal the
// 5-byte entry into a trampoline continuation that ends in an absolute jump
// back (REL::Asm::Jump14), and turn the entry into a REL::Asm::Jump5 to the
// thunk.
//
// The AV fork's built-in HookJump5/HookCall5 only chain onto targets that
// already begin with a branch (their old-function lookup decodes the opening
// E8/E9), and HookRepl writes raw bytes without a continuation — so the
// prologue variant derives from the fork's REL::IHookFn base and reuses its
// byte-capture / WriteSafe / Enable machinery plus its trampoline accounting.

namespace
{
	using ShowSubtitle_t = void(RE::SubtitleManager*, RE::TESObjectREFR*, const RE::BSFixedStringCS&, RE::TESTopicInfo*, bool);

	// Address Library variant ID (AV house pattern, cf. RE/IDs.hpp).
	//   OG 1.10.163 = 875508  (verified in the user's version-1-10-163-0.bin,
	//                          offset 0x12b2f70)
	//   NG 1.10.984 = 2249542
	//   AE 1.11.191 = 2249542  (reused NG value — UNVERIFIED; user runtime is OG)
	// AV's Iddb has no soft-failure path: an address lookup of an ID missing
	// from the installed DB is a hard REX::Fail (TerminateProcess) — the C8
	// CTD was exactly that. Never collapse this to a single-runtime ID, and
	// never add an engine call whose OG ID is not verified present.
	constexpr auto kShowSubtitle = F4SE::CreateVariantId(875508, 2249542, 2249542);

	constexpr std::size_t kStolenBytes{ sizeof(REL::Asm::Jump5) };

	F4DH::Application::ISubtitleSink* g_sink{ nullptr };
	ShowSubtitle_t*                   g_original{ nullptr };

	class HookPrologue5 final : public REL::IHookFn<ShowSubtitle_t>
	{
	public:
		using value_type = std::decay_t<ShowSubtitle_t>;

		HookPrologue5(std::string a_name, const REL::IId& a_id, const value_type& a_newFunc) :
			IHookFn(std::move(a_name), a_id, 0, a_newFunc)
		{
			InitData();
		}

		~HookPrologue5() noexcept override = default;

		HookPrologue5(const HookPrologue5&) = delete;
		HookPrologue5(HookPrologue5&&) noexcept = default;
		HookPrologue5& operator=(const HookPrologue5&) = delete;
		HookPrologue5& operator=(HookPrologue5&&) noexcept = default;

		[[nodiscard]] std::string_view GetTypeName() const noexcept override { return std::string_view{ "Prologue5" }; }

		// Callable original: the trampoline continuation built in InitNewBytes.
		[[nodiscard]] value_type GetContinuation() const noexcept { return _continuation; }

	protected:
		void InitNewBytes() override
		{
			// Entry -> thunk, through a trampoline branch allocated on install.
			const auto assembly = REL::Asm::Jump5(
				this->_address,
				REL::GetTrampoline()->AllocateBranch5(this->_newFunc.GetAddress()));
			REL::WriteData(std::span(this->_newBytes), assembly);

			// Continuation: the stolen entry bytes captured by IHookFn::Init,
			// followed by an absolute jump back to entry+5.
			auto* continuation = std::bit_cast<std::byte*>(_continuation);
			std::memcpy(continuation, this->_oldBytes.data(), kStolenBytes);
			const REL::Asm::Jump14 jmpBack(this->_address + kStolenBytes);
			std::memcpy(continuation + kStolenBytes, &jmpBack, sizeof(jmpBack));
		}

	private:
		void InitData()
		{
			this->_size = kStolenBytes;
			this->_trampolineSize = sizeof(REL::Asm::Jump14) + kStolenBytes + sizeof(REL::Asm::Jump14);

			// The address must exist before IHookFn::Init validates _oldFunc;
			// the bytes are filled in InitNewBytes, once _oldBytes is captured.
			const auto continuation = REL::GetTrampoline()->Allocate(kStolenBytes + sizeof(REL::Asm::Jump14));
			const auto continuationAddr = std::bit_cast<std::uintptr_t>(continuation);
			_continuation = std::bit_cast<value_type>(continuationAddr);
			this->_oldFunc = continuationAddr;
		}

		value_type _continuation{ nullptr };
	};

	[[nodiscard]] const char* ResolveSpeakerName(RE::TESObjectREFR* a_speaker, RE::TESTopicInfo* a_topicInfo)
	{
		// OG-safe order (northaxosky's FloatingSubtitlesF4 port): scene-speaker
		// full name, then the actor base's short name, else no name. No
		// display-name engine calls: GetDisplayFullName is absent from the OG
		// Address Library DB (hard Fail) and documented crash-prone on OG
		// regardless (BSResource::EntryDB::Force).
		if (a_topicInfo) {
			if (const auto speakerBase = a_topicInfo->GetSpeaker()) {
				if (const char* name = speakerBase->GetFullName(); name && name[0] != '\0') {
					return name;
				}
			}
		}

		// FormType check doubles as the cast guard.
		if (a_speaker && a_speaker->Is(RE::FormType::kActor)) {
			const auto actor = static_cast<RE::Actor*>(a_speaker);
			if (const auto npc = actor->GetActorBase()) {
				return npc->GetShortName();
			}
		}
		return nullptr;
	}

	[[nodiscard]] bool IsPlayerDialogueScene(RE::TESTopicInfo* a_topicInfo)
	{
		if (!a_topicInfo) {
			return false;
		}
		const auto scene = a_topicInfo->GetScene();
		// AV fork BGSScene::Flags values are masks (kPlayerDialogue = 1 << 5),
		// unlike the pinned libxse fork's bit indices — test the flag directly.
		return scene && scene->flags.any(RE::BGSScene::Flags::kPlayerDialogue);
	}

	void thunk(RE::SubtitleManager* a_manager, RE::TESObjectREFR* a_speaker, const RE::BSFixedStringCS& a_text, RE::TESTopicInfo* a_topicInfo, bool a_spokenToPlayer)
	{
		// Vanilla behavior first — a failure below must never break subtitles.
		g_original(a_manager, a_speaker, a_text, a_topicInfo, a_spokenToPlayer);

		// C10-DIAG (temporary): one line per ShowSubtitle call — resolves
		// C10-H5 (is the capture path firing at all?).
		{
			const char*       text = a_text.c_str();
			const std::string_view snippet = text ?
				std::string_view{ text }.substr(0, 40) :
				std::string_view{ "<null>" };
			REX::LogInformation("SubtitleHook: ShowSubtitle call speaker={:p} text='{}'",
				static_cast<const void*>(a_speaker), snippet);
		}

		if (!g_sink) {
			return;
		}

		F4DH::Core::SubtitleEvent event;
		event.speakerIsPlayer = a_speaker && a_speaker->IsPlayerRef();
		if (const char* name = ResolveSpeakerName(a_speaker, a_topicInfo)) {
			event.speakerName = name;
		}
		event.text = a_text.c_str() ? a_text.c_str() : "";
		const auto topicManager = RE::MenuTopicManager::GetSingleton();
		event.menuOpen = topicManager ? topicManager->menuOpen : false;
		event.sceneIsPlayerDialogue = IsPlayerDialogueScene(a_topicInfo);
		event.spokenToPlayer = a_spokenToPlayer;

		g_sink->OnSubtitle(event);
	}
}

namespace F4DH::Infrastructure
{
	bool SubtitleHook::Install(Application::ISubtitleSink& a_sink)
	{
		if (g_sink) {
			return true;  // idempotent — installed for process lifetime
		}

		// No missing-ID pre-check is possible: AV's Iddb hard-fails
		// (TerminateProcess) inside any address lookup of an ID absent from
		// the installed DB, before any comparison could run. Correctness rests
		// on the variant ID's OG slot being verified (see kShowSubtitle).

		auto& trampoline = REL::GetTrampoline();
		if (trampoline->IsEmpty()) {
			trampoline->Create(1 << 8);  // self-allocated; F4SE branch pool untouched
		}

		static std::shared_ptr<HookPrologue5> hook;
		hook = std::make_shared<HookPrologue5>("ShowSubtitle", kShowSubtitle, &thunk);
		if (!hook->Init()) {
			REX::LogError("SubtitleHook: failed to install ShowSubtitle detour (Address Library)");
			return false;
		}

		// Arm the original before enabling the detour so no intercepted call
		// can route into a null continuation.
		g_original = hook->GetContinuation();
		if (!hook->Enable()) {
			REX::LogError("SubtitleHook: failed to enable ShowSubtitle detour (Address Library)");
			return false;
		}
		g_sink = &a_sink;

		REX::LogInformation("SubtitleHook: ShowSubtitle detour installed (Address Library)");
		return true;
	}
}
