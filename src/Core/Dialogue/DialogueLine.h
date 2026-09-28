#pragma once

#include <cstdint>
#include <string>

namespace F4DH::Core
{
	enum class SpeakerKind : std::uint8_t
	{
		Player,
		Npc,
		Unknown,
	};

	// One recorded conversation line. Value object: identity is positional
	// within DialogueBuffer.
	struct DialogueLine
	{
		std::string   speaker;
		SpeakerKind   kind = SpeakerKind::Unknown;
		std::string   text;
		std::uint32_t questId = 0;  // owning quest formID, 0 = unattributed
		std::string   questName;    // owning quest name, empty = unattributed
		std::uint8_t  questType = 0;  // raw QUEST_DATA.type, 0 = kNone
	};
}
