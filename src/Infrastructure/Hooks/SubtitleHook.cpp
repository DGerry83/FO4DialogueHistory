#include "SubtitleHook.h"

// Milestone 3: implement the detour on REL::ID(2249542)
// (SubtitleManager::ShowSubtitle) following the proven pattern in
// powerof3/FloatingSubtitlesF4 (MIT) — see
// .flybywire/active/2026-09-27_DesignSpec_DialogueHistory/RESEARCH_NOTES.md.
//
// Translation notes for the implementer:
// - speakerIsPlayer: speaker->IsPlayerRef()
// - speakerName: topicInfo->GetSpeaker()->GetFullName(), else
//   ref->GetDisplayFullName(), else Actor -> TESNPC::GetShortName()
// - menuOpen: RE::MenuTopicManager::GetSingleton()->menuOpen
// - sceneIsPlayerDialogue: topicInfo->GetScene() &&
//   (scene->flags & BGSScene::FLAG::kPlayerDialogue)
// - spokenToPlayer: the hook's bool argument
// Hook-path constraints: O(1) work, no disk I/O, no unbounded allocation.

namespace F4DH::Infrastructure
{
	bool SubtitleHook::Install(Application::ISubtitleSink&)
	{
		return false;  // stub — milestone 3
	}
}
