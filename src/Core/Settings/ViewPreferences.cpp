#include "ViewPreferences.h"

#include <charconv>
#include <format>
#include <system_error>

namespace
{
	constexpr int kMinFontSize{ 1 };
	constexpr int kMaxFontSize{ 128 };

	constexpr bool IsJsonBoundary(char a_c)
	{
		return a_c == ',' || a_c == '}' || a_c == ' ' || a_c == '\t' || a_c == '\r' || a_c == '\n';
	}

	// Finds "key" : <value> in the payload, advancing past the colon and any
	// surrounding whitespace. Returns false when the key or colon is absent.
	[[nodiscard]] bool FindField(std::string_view a_json, std::string_view a_key, std::size_t& a_valuePos)
	{
		const std::string quoted = std::format("\"{}\"", a_key);
		const auto        pos = a_json.find(quoted);
		if (pos == std::string_view::npos) {
			return false;
		}

		auto i = pos + quoted.size();
		while (i < a_json.size()) {
			const char c = a_json[i];
			if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
				break;
			}
			i += 1;
		}
		if (i >= a_json.size() || a_json[i] != ':') {
			return false;
		}
		i += 1;
		while (i < a_json.size()) {
			const char c = a_json[i];
			if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
				break;
			}
			i += 1;
		}
		a_valuePos = i;
		return true;
	}

	// Reads an integer field: the token must be consumed up to a JSON
	// boundary, so fractional ("16.5"), exponent ("1e3"), and suffixed
	// ("16x") forms are all rejected. Range-checked against 1–128.
	[[nodiscard]] bool ParseFontSizeField(std::string_view a_json, int& a_out)
	{
		std::size_t i = 0;
		if (!FindField(a_json, "fontSize", i)) {
			return false;
		}

		std::size_t end = i;
		while (end < a_json.size() && !IsJsonBoundary(a_json[end])) {
			end += 1;
		}
		const auto token = a_json.substr(i, end - i);
		if (token.empty()) {
			return false;
		}

		unsigned int parsed = 0;
		const auto* const first = token.data();
		const auto* const last  = first + token.size();
		const auto result = std::from_chars(first, last, parsed, 10);
		// Unsigned parse rejects '-'; anything but full-token consumption
		// ('.', 'e', trailing letters) fails here.
		if (result.ec != std::errc{} || result.ptr != last) {
			return false;
		}
		if (parsed < kMinFontSize || parsed > kMaxFontSize) {
			return false;
		}
		a_out = static_cast<int>(parsed);
		return true;
	}

	[[nodiscard]] bool ParsePauseOnOpenField(std::string_view a_json, bool& a_out)
	{
		std::size_t i = 0;
		if (!FindField(a_json, "pauseOnOpen", i)) {
			return false;
		}

		const auto matchLiteral = [&a_json](std::size_t a_pos, std::string_view a_literal) {
			if (a_json.substr(a_pos, a_literal.size()) != a_literal) {
				return false;
			}
			const auto end = a_pos + a_literal.size();
			if (end != a_json.size() && !IsJsonBoundary(a_json[end])) {
				return false;  // literal ran into junk (e.g. "trueX", "false1")
			}
			return true;
		};

		if (matchLiteral(i, "true")) {
			a_out = true;
			return true;
		}
		if (matchLiteral(i, "false")) {
			a_out = false;
			return true;
		}
		return false;
	}
}

namespace F4DH::Core
{
	bool ParseViewPreferences(std::string_view a_json, ViewPreferences& a_inOut)
	{
		bool anyField = false;
		anyField = ParseFontSizeField(a_json, a_inOut.fontSize) || anyField;
		anyField = ParsePauseOnOpenField(a_json, a_inOut.pauseOnOpen) || anyField;
		return anyField;
	}

	std::string FormatViewPreferences(const ViewPreferences& a_preferences)
	{
		return std::format("{{\"fontSize\":{},\"pauseOnOpen\":{}}}",
			a_preferences.fontSize, a_preferences.pauseOnOpen);
	}
}
