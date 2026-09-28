#pragma once

#include <cstdint>
#include <string>

namespace RE
{
	class TESTopicInfo;
}

namespace F4DH::Infrastructure
{
	// Quest attribution for one subtitle event: 0/empty = unattributed.
	struct QuestResolution
	{
		std::uint32_t questId = 0;
		std::string   questName;
	};

	// Resolves the owning quest of a topic info: topicInfo->GetScene() ->
	// BGSScene::parentQuest preferred, parentTopic -> TESTopic::ownerQuest
	// fallback. GetScene is the only engine call (Address Library variant ID,
	// OG 820897); every other hop is a member read or a virtual. Raw
	// "<alias=...>" placeholders in the quest's fullName are substituted from
	// the quest's filled aliases where possible (M7; GetAliasedRef is
	// OG-verified REL::ID 847223); unresolvable tokens stay verbatim.
	// Game-thread only: bounded work, no I/O, no locks. Newly seen quests are
	// logged once per distinct questId from the Resolve path.
	namespace QuestResolver
	{
		[[nodiscard]] QuestResolution Resolve(RE::TESTopicInfo* a_topicInfo);
	}
}
