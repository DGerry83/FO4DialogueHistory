#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "DialogueLine.h"

namespace F4DH::Core
{
	// Binary codec for the cosave 'HIST' record payload. Engine-free; format v1
	// (the record-level version is the F4SE record version, not embedded here):
	//   payload = uint32 lineCount, then per line —
	//   uint8 kind | uint32 questId |
	//   uint32 speakerLen + bytes | uint32 textLen + bytes |
	//   uint32 questNameLen + bytes
	// All integers little-endian. Decode never throws and never reads out of
	// bounds; on malformed/truncated input it returns false with whatever
	// lines decoded before the defect left in a_outLines — a caller that
	// clears first and only rebuilds on success gets an all-or-nothing load.
	// The count is never pre-reserved, so a corrupt count cannot become an
	// allocation bomb; each line is validated as it is read.
	class CosaveCodec
	{
	public:
		[[nodiscard]] static std::vector<std::byte> Encode(const std::vector<DialogueLine>& a_lines);
		[[nodiscard]] static bool Decode(std::span<const std::byte> a_data, std::vector<DialogueLine>& a_outLines);
	};
}
