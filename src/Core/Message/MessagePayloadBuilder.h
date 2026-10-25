#pragma once

#include <string>
#include <vector>

#include "MessageLine.h"

namespace F4DH::Core
{
	// Serializes message history into the JSON payloads consumed by the
	// PrismaUI view (setMessageHistory / appendMessage). Snapshot is an
	// oldest-first array; append is one {"text":"..."} object. String
	// escaping is identical to the dialogue payload builder.
	class MessagePayloadBuilder
	{
	public:
		[[nodiscard]] static std::string BuildSnapshot(const std::vector<MessageLine>& a_messages);
		[[nodiscard]] static std::string BuildAppend(const MessageLine& a_message);

	private:
		[[nodiscard]] static std::string Escape(const std::string& value);
	};
}
