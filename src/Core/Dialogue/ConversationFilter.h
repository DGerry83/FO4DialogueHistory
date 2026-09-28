#pragma once

#include "SubtitleEvent.h"

namespace F4DH::Core
{
	// Pure classification rule: is this subtitle event part of a
	// player-involved conversation scene, or quest-owned dialogue from a
	// typed (Pip-Boy) quest? The quest-type branch admits scene-less staged
	// quest performances (C4); bark-holder quests are type-less (kNone) and
	// stay rejected, and the player-speaker exclusion keeps solo player
	// voice lines (user ruling 2026-09-28: wontfix) out.
	namespace ConversationFilter
	{
		[[nodiscard]] inline bool IsConversationDialogue(const SubtitleEvent& event)
		{
			return event.menuOpen || event.sceneIsPlayerDialogue ||
				(event.speakerIsPlayer && event.spokenToPlayer) ||
				(!event.speakerIsPlayer && event.questType != 0);
		}
	}
}
