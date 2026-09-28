#include "QuestResolver.h"

#include "RE/B/BGSScene.hpp"
#include "RE/T/TESQuest.hpp"
#include "RE/T/TESTopic.hpp"
#include "RE/T/TESTopicInfo.hpp"

#include "REX/Log.hpp"

#include <unordered_set>

namespace
{
	void LogOncePerQuest(std::uint32_t a_questId, const std::string& a_questName, const char* a_via)
	{
		// Game-thread only (called from the ShowSubtitle thunk path) — the
		// seen-set needs no lock. Bounded by distinct quests, not line count.
		static std::unordered_set<std::uint32_t> seen;
		if (!seen.insert(a_questId).second) {
			return;
		}
		REX::LogInformation("quest resolved: '{}' (id {:08X}) via {}", a_questName, a_questId, a_via);
	}
}

namespace F4DH::Infrastructure
{
	QuestResolution QuestResolver::Resolve(RE::TESTopicInfo* a_topicInfo)
	{
		QuestResolution result;
		if (!a_topicInfo) {
			return result;
		}

		// The only engine call on this path (C8 rule: OG-verified variant ID).
		const auto scene = a_topicInfo->GetScene();

		RE::TESQuest* quest = nullptr;
		const char*   via = nullptr;
		if (scene && scene->parentQuest) {
			quest = scene->parentQuest;
			via = "scene";
		} else if (const auto topic = a_topicInfo->parentTopic; topic && topic->ownerQuest) {
			quest = topic->ownerQuest;
			via = "topic";
		}
		if (!quest) {
			return result;
		}

		result.questId = quest->formID;
		if (const char* name = quest->fullName.data(); name && name[0] != '\0') {
			result.questName = name;
		} else if (const char* editorID = quest->GetFormEditorID(); editorID && editorID[0] != '\0') {
			result.questName = editorID;
		}

		LogOncePerQuest(result.questId, result.questName, via);
		return result;
	}
}
