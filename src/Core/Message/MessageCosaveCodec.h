#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "MessageLine.h"

namespace F4DH::Core
{
	// Binary codec for the cosave 'MSGS' record payload. Engine-free; the
	// record-level version (the F4SE record version, not embedded here)
	// selects the layout:
	//   v1: payload = uint32 messageCount, then per message —
	//       uint32 textLen + bytes
	// All integers little-endian. Decode never throws and never reads out of
	// bounds; on malformed/truncated input it returns false with whatever
	// messages decoded before the defect left in a_outMessages — a caller
	// that clears first and only rebuilds on success gets an all-or-nothing
	// load. The count is never pre-reserved, so a corrupt count cannot
	// become an allocation bomb; each message is validated as it is read.
	// Text bytes are carried verbatim (UTF-8 by contract; well-formedness is
	// not validated).
	class MessageCosaveCodec
	{
	public:
		[[nodiscard]] static std::vector<std::byte> Encode(const std::vector<MessageLine>& a_messages);
		[[nodiscard]] static bool Decode(std::span<const std::byte> a_data, std::vector<MessageLine>& a_outMessages, std::uint32_t a_version);
	};
}
