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
		std::string  speaker;
		SpeakerKind  kind = SpeakerKind::Unknown;
		std::string  text;
	};
}
