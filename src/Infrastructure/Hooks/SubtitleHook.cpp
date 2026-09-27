#include "SubtitleHook.h"

#include "Application/Capture/ISubtitleSink.h"
#include "Core/Dialogue/SubtitleEvent.h"

#include "REL/ASM.h"
#include "REL/Trampoline.h"
#include "REL/Utility.h"

#include "RE/A/Actor.h"
#include "RE/B/BGSScene.h"
#include "RE/E/ENUM_FORM_ID.h"
#include "RE/E/ExtraTextDisplayData.h"
#include "RE/M/MenuTopicManager.h"
#include "RE/T/TESNPC.h"
#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESTopicInfo.h"

#include "REX/LOG.h"

#include <cstring>

// Prologue detour on SubtitleManager::ShowSubtitle (Address Library
// REL::ID 2249542), the pattern proven by powerof3/FloatingSubtitlesF4 and
// Agent69-stack/FallHook: steal 5 bytes into the trampoline, continue with a
// JMP14 back to target+5, and overwrite the entry with a JMP5 to the thunk.
// clib-util's stl::hook_function_prologue (the historical helper with these
// exact mechanics) is no longer published, so the same byte-level steps are
// performed here with pinned-fork primitives only.

namespace
{
	using ShowSubtitle_t = void(RE::SubtitleManager*, RE::TESObjectREFR*, const RE::BSFixedStringCS&, RE::TESTopicInfo*, bool);

	constexpr REL::ID          kShowSubtitle{ 2249542 };
	constexpr std::size_t      kStolenBytes{ 5 };
	constexpr std::uintptr_t   kPlayerDialogueBit{ 1u << std::to_underlying(RE::BGSScene::FLAG::kPlayerDialogue) };

	F4DH::Application::ISubtitleSink* g_sink{ nullptr };
	ShowSubtitle_t*                   g_original{ nullptr };

	[[nodiscard]] const char* ResolveSpeakerName(RE::TESObjectREFR* a_speaker, RE::TESTopicInfo* a_topicInfo)
	{
		if (a_topicInfo) {
			if (const auto speakerBase = a_topicInfo->GetSpeaker()) {
				if (const char* name = speakerBase->GetFullName(); name && name[0] != '\0') {
					return name;
				}
			}
		}

		if (!a_speaker) {
			return nullptr;
		}

		if (!a_speaker->Is(RE::ENUM_FORM_ID::kACHR) ||
			(a_speaker->extraList && a_speaker->extraList->HasType<RE::ExtraTextDisplayData>())) {
			return a_speaker->GetDisplayFullName();
		}

		const auto actor = static_cast<RE::Actor*>(a_speaker);
		if (const auto npc = actor->GetNPC()) {
			return npc->GetShortName();
		}
		return nullptr;
	}

	[[nodiscard]] bool IsPlayerDialogueScene(RE::TESTopicInfo* a_topicInfo)
	{
		if (!a_topicInfo) {
			return false;
		}
		const auto scene = a_topicInfo->GetScene();
		return scene && (scene->flags & kPlayerDialogueBit) != 0;
	}

	void thunk(RE::SubtitleManager* a_manager, RE::TESObjectREFR* a_speaker, const RE::BSFixedStringCS& a_text, RE::TESTopicInfo* a_topicInfo, bool a_spokenToPlayer)
	{
		// Vanilla behavior first — a failure below must never break subtitles.
		g_original(a_manager, a_speaker, a_text, a_topicInfo, a_spokenToPlayer);

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

		auto& trampoline = REL::GetTrampoline();
		if (trampoline.empty()) {
			trampoline.create(1 << 8);  // self-allocated; F4SE branch pool untouched
		}

		const auto target = kShowSubtitle.address();

		// Continuation: stolen prologue bytes + absolute jump back to target+5.
		auto* continuation = static_cast<std::byte*>(trampoline.allocate(kStolenBytes + sizeof(REL::ASM::JMP14)));
		std::memcpy(continuation, reinterpret_cast<const void*>(target), kStolenBytes);
		const REL::ASM::JMP14 jmpBack(target + kStolenBytes);
		std::memcpy(continuation + kStolenBytes, &jmpBack, sizeof(jmpBack));

		g_original = reinterpret_cast<ShowSubtitle_t*>(continuation);
		g_sink = &a_sink;

		const REL::ASM::JMP5 jmpThunk(target, reinterpret_cast<std::uintptr_t>(&thunk));
		if (!REL::WriteSafe(target, static_cast<const void*>(&jmpThunk), sizeof(jmpThunk))) {
			REX::ERROR("SubtitleHook: failed to patch ShowSubtitle at REL::ID(2249542)");
			return false;
		}

		REX::INFO("SubtitleHook: ShowSubtitle detour installed (REL::ID 2249542)");
		return true;
	}
}
