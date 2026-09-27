#pragma once

#include "SubtitleEvent.h"

namespace F4DH::Core
{
	// Pure classification rule: is this subtitle event part of a
	// player-involved conversation scene?
	namespace ConversationFilter
	{
		[[nodiscard]] inline bool IsConversationDialogue(const SubtitleEvent& event)
		{
			return event.menuOpen || event.sceneIsPlayerDialogue ||
				(event.speakerIsPlayer && event.spokenToPlayer);
		}
	}
}
