#pragma once

#include <string>
#include <vector>

#include "DialogueLine.h"

namespace F4DH::Core
{
	// Serializes dialogue history into the JSON payloads consumed by the
	// PrismaUI view. Snapshot is an oldest-first array; append is one object.
	class PayloadBuilder
	{
	public:
		[[nodiscard]] static std::string BuildSnapshot(const std::vector<DialogueLine>& lines);
		[[nodiscard]] static std::string BuildAppend(const DialogueLine& line);

	private:
		[[nodiscard]] static std::string Escape(const std::string& value);
		[[nodiscard]] static std::string LineToJson(const DialogueLine& line);
	};
}
