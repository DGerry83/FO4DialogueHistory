#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace F4DH::Core
{
	// Substitution for Fallout 4 quest-text placeholder tokens of the form
	// "<alias=name>" (with optional property suffix: "<alias=name.ShortName>").
	// The engine resolves these at display time from the quest's filled
	// aliases; raw TESQuest::fullName keeps them verbatim. The token keyword
	// match is case-insensitive; the alias name is everything up to '.'
	// or '>'. A lookup miss or a malformed token (no closing '>') keeps the
	// source text verbatim.
	namespace AliasTokens
	{
		namespace detail
		{
			[[nodiscard]] constexpr bool IEqual(char a, char b) noexcept
			{
				const auto lower = [](char c) constexpr noexcept {
					return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
				};
				return lower(a) == lower(b);
			}

			// Start of the next "<alias=" token at or after `from`, or npos.
			[[nodiscard]] constexpr std::size_t FindTokenStart(std::string_view text, std::size_t from) noexcept
			{
				constexpr std::string_view kPrefix = "<alias=";
				for (std::size_t i = from; i + kPrefix.size() <= text.size(); ++i) {
					if (text[i] != '<') {
						continue;
					}
					bool match = true;
					for (std::size_t j = 0; j < kPrefix.size(); ++j) {
						if (!IEqual(text[i + j], kPrefix[j])) {
							match = false;
							break;
						}
					}
					if (match) {
						return i;
					}
				}
				return std::string_view::npos;
			}
		}

		// Returns a copy of `text` with every resolvable alias token replaced.
		// `lookup` receives the base alias name (property suffix stripped) and
		// returns the display name, or std::nullopt to keep the token verbatim.
		template <class Lookup>
		[[nodiscard]] std::string Substitute(std::string_view text, Lookup&& lookup)
		{
			std::string out;
			out.reserve(text.size());

			std::size_t cursor = 0;
			while (cursor < text.size()) {
				const auto start = detail::FindTokenStart(text, cursor);
				if (start == std::string_view::npos) {
					break;
				}

				const auto bodyStart = start + 7;  // strlen("<alias=")
				const auto close = text.find('>', bodyStart);
				if (close == std::string_view::npos) {
					break;  // malformed token — keep the remainder verbatim
				}

				std::string_view body = text.substr(bodyStart, close - bodyStart);
				const auto dot = body.find('.');
				const std::string_view name = (dot == std::string_view::npos) ? body : body.substr(0, dot);

				out.append(text.substr(cursor, start - cursor));
				if (!name.empty()) {
					if (auto resolved = lookup(name)) {
						out.append(*resolved);
						cursor = close + 1;
						continue;
					}
				}
				// Unresolvable — keep the full token text verbatim.
				out.append(text.substr(start, close + 1 - start));
				cursor = close + 1;
			}

			out.append(text.substr(cursor));
			return out;
		}

		[[nodiscard]] inline bool ContainsToken(std::string_view text) noexcept
		{
			return detail::FindTokenStart(text, 0) != std::string_view::npos;
		}
	}
}
