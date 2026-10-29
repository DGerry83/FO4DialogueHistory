#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace F4DH::Core
{
	struct NoteSearchDocument
	{
		std::uint64_t entryId{ 0 };
		std::string title;
		std::string body;
	};

	class NoteSearch
	{
	public:
		[[nodiscard]] static bool Matches(std::string_view a_title, std::string_view a_body, std::string_view a_term);
		[[nodiscard]] static std::vector<std::uint64_t> Filter(const std::vector<NoteSearchDocument>& a_documents, std::string_view a_term);
	};
}
