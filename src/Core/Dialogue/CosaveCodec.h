#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "DialogueLine.h"

namespace F4DH::Core
{
	// Binary codec for the cosave 'HIST' record payload. Engine-free; the
	// record-level version (the F4SE record version, not embedded here) selects
	// the layout:
	//   v1: payload = uint32 lineCount, then per line —
	//       uint8 kind | uint32 questId |
	//       uint32 speakerLen + bytes | uint32 textLen + bytes |
	//       uint32 questNameLen + bytes
	//   v2: same as v1 plus one trailing uint8 questType per line (after
	//       questName)
	// All integers little-endian. Decode never throws and never reads out of
	// bounds; on malformed/truncated input it returns false with whatever
	// lines decoded before the defect left in a_outLines — a caller that
	// clears first and only rebuilds on success gets an all-or-nothing load.
	// The count is never pre-reserved, so a corrupt count cannot become an
	// allocation bomb; each line is validated as it is read. v1 lines decode
	// with questType 0 (kNone).
	class CosaveCodec
	{
	public:
		[[nodiscard]] static std::vector<std::byte> Encode(const std::vector<DialogueLine>& a_lines);
		[[nodiscard]] static bool Decode(std::span<const std::byte> a_data, std::vector<DialogueLine>& a_outLines, std::uint32_t a_version);
	};
}
