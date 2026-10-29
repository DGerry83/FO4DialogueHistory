#include "NoteTitleGenerator.h"

#include "NoteTextFormatter.h"

#include <algorithm>
#include <cctype>

namespace
{
	std::string Lower(std::string_view a_value)
	{
		std::string out(a_value);
		std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return out;
	}

	bool Same(std::string_view a_left, std::string_view a_right)
	{
		return Lower(a_left) == Lower(a_right);
	}

	bool Contains(const std::vector<std::string>& a_values, std::string_view a_value)
	{
		return std::ranges::any_of(a_values, [&](const std::string& value) { return Same(value, a_value); });
	}

	std::string Opening(std::string_view a_body)
	{
		const auto normalized = F4DH::Core::NoteTextFormatter::NormalizeOpening(a_body);
		if (normalized.empty()) return {};

		std::size_t bytes = 0;
		std::size_t points = 0;
		while (bytes < normalized.size() && points < 64) {
			const auto c = static_cast<unsigned char>(normalized[bytes]);
			std::size_t width = 1;
			if ((c & 0xE0) == 0xC0) width = 2;
			else if ((c & 0xF0) == 0xE0) width = 3;
			else if ((c & 0xF8) == 0xF0) width = 4;
			if (bytes + width > normalized.size()) break;
			bytes += width;
			++points;
		}
		if (bytes == normalized.size()) return normalized;

		std::size_t end = bytes;
		const auto space = normalized.rfind(' ', end);
		if (space != std::string::npos && space >= 32) end = space;
		return normalized.substr(0, end) + "…";
	}
}

namespace F4DH::Core
{
	std::string NoteTitleGenerator::Generate(
		std::string_view a_originalTitle,
		std::string_view a_plainBody,
		const std::vector<std::string>& a_existingTitles,
		const std::vector<std::string>& a_genericTitles)
	{
		const bool rawAlias = NoteTextFormatter::ContainsUnresolvedAlias(a_originalTitle);
		const std::string base = rawAlias ? "Note" : a_originalTitle.empty() ? "Untitled note" : std::string(a_originalTitle);
		const bool generic = rawAlias || Contains(a_genericTitles, base);
		std::string candidate = base;
		if (generic || Contains(a_existingTitles, candidate)) {
			const auto opening = Opening(a_plainBody);
			if (!opening.empty()) candidate = base + ": " + opening;
		}

		if (!Contains(a_existingTitles, candidate)) return candidate;
		for (std::uint32_t suffix = 2;; ++suffix) {
			const auto numbered = candidate + " (" + std::to_string(suffix) + ")";
			if (!Contains(a_existingTitles, numbered)) return numbered;
		}
	}
}
