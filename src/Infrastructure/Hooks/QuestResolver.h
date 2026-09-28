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
		std::int32_t  questType = 0;  // raw QUEST_DATA.type, 0 = kNone
	};

	// Resolves the owning quest of a topic info: topicInfo->GetScene() ->
	// BGSScene::parentQuest preferred, parentTopic -> TESTopic::ownerQuest
	// fallback. GetScene is the only engine call on the resolve path (Address
	// Library variant ID, OG 820897); every other hop is a member read or a
	// virtual. Raw "<alias=...>" placeholders in the quest's fullName are
	// substituted from the quest's alias and instance data (M7; GetAliasedRef
	// OG 847223, proven in-session); unresolvable tokens stay verbatim.
	// Multi-threaded hook path (U1b): bounded work, no I/O; the log-dedup
	// seen-sets take a bounded lock held only for check+insert. Newly seen
	// quests are logged once per distinct questId from the Resolve path.
	namespace QuestResolver
	{
		[[nodiscard]] QuestResolution Resolve(RE::TESTopicInfo* a_topicInfo);
	}
}
