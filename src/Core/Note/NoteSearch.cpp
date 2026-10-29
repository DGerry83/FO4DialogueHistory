#include "NoteSearch.h"

#include <algorithm>
#include <cctype>

namespace
{
	std::string Normalize(std::string_view a_value)
	{
		std::string out;
		out.reserve(a_value.size());
		bool pendingSpace = false;
		for (const unsigned char c : a_value) {
			if (std::isspace(c)) {
				pendingSpace = !out.empty();
				continue;
			}
			if (pendingSpace) {
				out.push_back(' ');
				pendingSpace = false;
			}
			out.push_back(static_cast<char>(std::tolower(c)));
		}
		return out;
	}

	bool Contains(std::string_view a_haystack, std::string_view a_needle)
	{
		return Normalize(a_haystack).find(a_needle) != std::string::npos;
	}
}

namespace F4DH::Core
{
	bool NoteSearch::Matches(std::string_view a_title, std::string_view a_body, std::string_view a_term)
	{
		const auto term = Normalize(a_term);
		if (term.empty()) return true;
		return Contains(a_title, term) || (!a_body.empty() && Contains(a_body, term));
	}

	std::vector<std::uint64_t> NoteSearch::Filter(const std::vector<NoteSearchDocument>& a_documents, std::string_view a_term)
	{
		std::vector<std::uint64_t> out;
		for (const auto& document : a_documents) {
			if (Matches(document.title, document.body, a_term)) out.push_back(document.entryId);
		}
		return out;
	}
}
