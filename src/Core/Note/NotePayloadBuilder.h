#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "NoteEntry.h"

namespace F4DH::Core
{
	class NotePayloadBuilder
	{
	public:
		[[nodiscard]] static std::string BuildList(const std::vector<NoteListItem>& a_items);
		[[nodiscard]] static std::string BuildTree(const std::vector<NoteFolder>& a_folders, const std::vector<NoteListItem>& a_items);
		[[nodiscard]] static std::string BuildOperationResult(const NoteOperation& a_result);
		[[nodiscard]] static std::string BuildBody(const NoteBodyResult& a_result);
		[[nodiscard]] static std::string Escape(std::string_view a_value);
	};
}
