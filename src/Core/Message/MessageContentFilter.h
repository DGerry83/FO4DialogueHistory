#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace F4DH::Core
{
	class MessageContentFilter
	{
	public:
		[[nodiscard]] static bool ShouldCapture(std::string_view a_text, const std::vector<std::string>& a_excludedPrefixes) noexcept
		{
			return !a_text.empty() && FindExcludedPrefix(a_text, a_excludedPrefixes).empty();
		}

		[[nodiscard]] static std::string_view FindExcludedPrefix(std::string_view a_text, const std::vector<std::string>& a_excludedPrefixes) noexcept
		{
			for (const auto& prefix : a_excludedPrefixes) {
				if (!prefix.empty() && a_text.starts_with(prefix)) {
					return prefix;
				}
			}
			return {};
		}

		[[nodiscard]] static std::vector<std::string> ParseExcludedPrefixes(const std::vector<std::string>& a_lines)
		{
			std::vector<std::string> prefixes;
			bool                     inMessageFiltersSection{ false };

			for (const auto& line : a_lines) {
				auto view = Trim(line);
				if (view.empty() || view.front() == ';' || view.front() == '#') {
					continue;
				}
				if (view.front() == '[' && view.back() == ']') {
					inMessageFiltersSection = ToLower(Trim(view.substr(1, view.size() - 2))) == "messagefilters";
					continue;
				}
				if (!inMessageFiltersSection) {
					continue;
				}

				const auto eq = view.find('=');
				if (eq == std::string_view::npos || ToLower(Trim(view.substr(0, eq))) != "excludedprefix") {
					continue;
				}
				const auto prefix = Trim(view.substr(eq + 1));
				if (!prefix.empty()) {
					prefixes.emplace_back(prefix);
				}
			}
			return prefixes;
		}

	private:
		[[nodiscard]] static std::string_view Trim(std::string_view a_value) noexcept
		{
			while (!a_value.empty() && (a_value.front() == ' ' || a_value.front() == '\t')) {
				a_value.remove_prefix(1);
			}
			while (!a_value.empty() && (a_value.back() == ' ' || a_value.back() == '\t')) {
				a_value.remove_suffix(1);
			}
			return a_value;
		}

		[[nodiscard]] static std::string ToLower(std::string_view a_value)
		{
			std::string result{ a_value };
			for (auto& c : result) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return result;
		}
	};
}
