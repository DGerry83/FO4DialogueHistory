#pragma once

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

namespace F4DH::Core::NoteFolderRules
{
	inline std::string NormalizeName(std::string_view a_input)
	{
		std::string out;
		out.reserve(a_input.size());
		bool pendingSpace = false;
		std::size_t codePoints = 0;
		for (std::size_t i = 0; i < a_input.size();) {
			const auto c = static_cast<unsigned char>(a_input[i]);
			if (std::isspace(c)) {
				pendingSpace = !out.empty();
				++i;
				continue;
			}
			if (pendingSpace) {
				out.push_back(' ');
				pendingSpace = false;
			}
			std::size_t width = 1;
			if ((c & 0xE0) == 0xC0) width = 2;
			else if ((c & 0xF0) == 0xE0) width = 3;
			else if ((c & 0xF8) == 0xF0) width = 4;
			if (width > 1 && (i + width > a_input.size() || (c & 0xF8) == 0xF8)) return {};
			for (std::size_t j = 1; j < width; ++j) {
				if ((static_cast<unsigned char>(a_input[i + j]) & 0xC0) != 0x80) return {};
			}
			if (++codePoints > 64) return {};
			out.append(a_input.substr(i, width));
			i += width;
		}
		return out;
	}

	inline bool SameName(std::string_view a_left, std::string_view a_right)
	{
		if (a_left.size() != a_right.size()) return false;
		for (std::size_t i = 0; i < a_left.size(); ++i) {
			if (std::tolower(static_cast<unsigned char>(a_left[i])) != std::tolower(static_cast<unsigned char>(a_right[i]))) return false;
		}
		return true;
	}
}
