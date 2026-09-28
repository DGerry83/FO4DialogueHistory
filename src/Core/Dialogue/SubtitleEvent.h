#pragma once

#include <cstdint>
#include <string>

namespace F4DH::Core
{
	// Engine-free translation of one SubtitleManager::ShowSubtitle call.
	// Infrastructure::SubtitleHook builds this from RE types; Core and
	// Application never see engine types.
	struct SubtitleEvent
	{
		bool        speakerIsPlayer = false;
		std::string speakerName;
		std::string text;
		bool        menuOpen = false;              // MenuTopicManager::menuOpen at capture time
		bool        sceneIsPlayerDialogue = false; // topicInfo scene has BGSScene::kPlayerDialogue
		bool        spokenToPlayer = false;        // ShowSubtitle's spokenToPlayer argument
		std::uint32_t questId = 0;                 // owning quest formID, 0 = unattributed
		std::string   questName;                   // owning quest name, empty = unattributed
	};
}
