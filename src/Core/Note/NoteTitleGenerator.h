#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace F4DH::Core
{
	class NoteTitleGenerator
	{
	public:
		[[nodiscard]] static std::string Generate(
			std::string_view a_originalTitle,
			std::string_view a_plainBody,
			const std::vector<std::string>& a_existingTitles,
			const std::vector<std::string>& a_genericTitles = { "Note", "Letter", "Ransom Note" });
	};
}
