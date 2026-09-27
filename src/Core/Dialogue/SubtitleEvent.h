#pragma once

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
	};
}
