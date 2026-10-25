#include "ViewPreferences.h"

#include <charconv>
#include <format>
#include <system_error>

namespace
{
	constexpr int kMinFontSize{ 1 };
	constexpr int kMaxFontSize{ 128 };
	// Payloads come from our own view; deeper nesting than this is treated
	// as malformed rather than skipped (bounded memory, wholesale reject).
	constexpr std::size_t kMaxNesting{ 64 };

	constexpr bool IsWs(char a_c)
	{
		return a_c == ' ' || a_c == '\t' || a_c == '\r' || a_c == '\n';
	}

	// A bare-token value (number, bool, null, junk word) ends at whitespace
	// or at the separators that can follow a value.
	constexpr bool IsTokenEnd(char a_c)
	{
		return IsWs(a_c) || a_c == ',' || a_c == '}';
	}

	[[nodiscard]] std::size_t SkipWs(std::string_view a_json, std::size_t a_i)
	{
		while (a_i < a_json.size() && IsWs(a_json[a_i])) {
			a_i += 1;
		}
		return a_i;
	}

	// Consumes a JSON string starting at its opening quote; escape-aware so
	// an escaped quote cannot terminate it early. Returns the index just past
	// the closing quote, or npos when unterminated.
	[[nodiscard]] std::size_t SkipString(std::string_view a_json, std::size_t a_i)
	{
		a_i += 1;  // opening quote
		while (a_i < a_json.size()) {
			const char c = a_json[a_i];
			if (c == '\\') {
				a_i += 2;
				continue;
			}
			if (c == '"') {
				return a_i + 1;
			}
			a_i += 1;
		}
		return std::string_view::npos;
	}

	// Consumes one JSON value (string / object / array / bare token) starting
	// at a_i so unknown members — and lookalike keys inside string values or
	// nested objects/arrays — are skipped structurally instead of matched as
	// text. Objects and arrays are validated with a small expected-closer
	// stack, so mixed/mismatched nesting (`[}]`) is malformed rather than
	// swallowed. Returns the index just past the value, or npos when
	// malformed (unterminated string, mismatched or unbalanced brackets,
	// nesting deeper than kMaxNesting). A missing value (the next character
	// is ',' or '}') consumes nothing.
	[[nodiscard]] std::size_t SkipValue(std::string_view a_json, std::size_t a_i)
	{
		if (a_i >= a_json.size() || IsTokenEnd(a_json[a_i])) {
			return a_i;
		}
		if (a_json[a_i] == '"') {
			return SkipString(a_json, a_i);
		}
		if (a_json[a_i] == '{' || a_json[a_i] == '[') {
			char        expected[kMaxNesting];
			std::size_t depth = 0;
			std::size_t i = a_i;
			while (i < a_json.size()) {
				const char c = a_json[i];
				if (c == '"') {
					const auto end = SkipString(a_json, i);
					if (end == std::string_view::npos) {
						return std::string_view::npos;
					}
					i = end;
					continue;
				}
				if (c == '{' || c == '[') {
					if (depth == kMaxNesting) {
						return std::string_view::npos;  // pathologically deep
					}
					expected[depth++] = (c == '{') ? '}' : ']';
					i += 1;
					continue;
				}
				if (c == '}' || c == ']') {
					if (depth == 0 || expected[depth - 1] != c) {
						return std::string_view::npos;  // mismatched or stray closer
					}
					depth -= 1;
					i += 1;
					if (depth == 0) {
						return i;
					}
					continue;
				}
				i += 1;
			}
			return std::string_view::npos;  // unbalanced
		}
		while (a_i < a_json.size() && !IsTokenEnd(a_json[a_i])) {
			a_i += 1;
		}
		return a_i;
	}

	// Validates a bare-token integer: full consumption (rejecting "16.5",
	// "1e3", "16x"), unsigned (rejecting "-8"), range 1–128. Assigns a_out
	// only on success.
	[[nodiscard]] bool ParseFontSizeToken(std::string_view a_json, std::size_t a_begin, std::size_t a_end, int& a_out)
	{
		const auto token = a_json.substr(a_begin, a_end - a_begin);
		if (token.empty()) {
			return false;
		}
		unsigned int parsed = 0;
		const auto* const first = token.data();
		const auto* const last  = first + token.size();
		const auto result = std::from_chars(first, last, parsed, 10);
		if (result.ec != std::errc{} || result.ptr != last) {
			return false;
		}
		if (parsed < kMinFontSize || parsed > kMaxFontSize) {
			return false;
		}
		a_out = static_cast<int>(parsed);
		return true;
	}

	// Validates a bare-token bool: the token must be exactly the JSON true/
	// false literal (a quoted "true" is the wrong type and keeps the seed).
	[[nodiscard]] bool ParseBoolToken(std::string_view a_json, std::size_t a_begin, std::size_t a_end, bool& a_out)
	{
		const auto token = a_json.substr(a_begin, a_end - a_begin);
		if (token == "true") {
			a_out = true;
			return true;
		}
		if (token == "false") {
			a_out = false;
			return true;
		}
		return false;
	}

	// Scans the payload as ONE flat JSON object, member by member, so only
	// actual top-level member keys are recognized: lookalikes inside string
	// values or nested objects/arrays are consumed as values and can never
	// shadow a real key, and a payload that is not a structurally valid
	// object (no wrapper, unterminated string/object, missing colon, junk
	// between members, trailing non-whitespace) fails wholesale — a
	// valid-looking field never legitimizes junk. A member whose value is the
	// wrong type is per-field tolerant: it leaves the seeded value in place
	// and scanning continues. Writes per-field results into a_inOut and sets
	// a_anyField when at least one field produced a valid value. Returns
	// false when the payload is structurally malformed (a_inOut may hold
	// partially scanned fields; the caller discards them).
	[[nodiscard]] bool ScanObject(std::string_view a_json, F4DH::Core::ViewPreferences& a_inOut, bool& a_anyField)
	{
		a_anyField = false;
		std::size_t i = SkipWs(a_json, 0);
		if (i >= a_json.size() || a_json[i] != '{') {
			return false;  // not a JSON object at all
		}
		i += 1;

		while (true) {
			i = SkipWs(a_json, i);
			if (i >= a_json.size()) {
				return false;  // unterminated object
			}
			if (a_json[i] == '}') {
				i += 1;
				break;
			}
			if (a_json[i] != '"') {
				return false;  // a member key must be a JSON string
			}
			const auto keyEnd = SkipString(a_json, i);
			if (keyEnd == std::string_view::npos) {
				return false;  // unterminated key
			}
			const auto key = a_json.substr(i + 1, keyEnd - i - 2);
			i = SkipWs(a_json, keyEnd);
			if (i >= a_json.size() || a_json[i] != ':') {
				return false;  // missing colon
			}
			i = SkipWs(a_json, i + 1);

			const auto valueEnd = SkipValue(a_json, i);
			if (valueEnd == std::string_view::npos) {
				return false;  // unterminated string/object/array value
			}
			if (key == "fontSize") {
				a_anyField = ParseFontSizeToken(a_json, i, valueEnd, a_inOut.fontSize) || a_anyField;
			} else if (key == "pauseOnOpen") {
				a_anyField = ParseBoolToken(a_json, i, valueEnd, a_inOut.pauseOnOpen) || a_anyField;
			}
			// Unknown keys and wrong-typed values are skipped without effect.

			i = SkipWs(a_json, valueEnd);
			if (i >= a_json.size()) {
				return false;  // unterminated object
			}
			if (a_json[i] == ',') {
				// A member must follow the comma — a trailing comma is not
				// valid JSON, so `{"fontSize":20,}` fails wholesale.
				const auto next = SkipWs(a_json, i + 1);
				if (next >= a_json.size() || a_json[next] == '}') {
					return false;
				}
				i = next;
				continue;
			}
			if (a_json[i] == '}') {
				i += 1;
				break;
			}
			return false;  // junk between members
		}

		i = SkipWs(a_json, i);
		return i == a_json.size();  // trailing non-whitespace disqualifies
	}
}

namespace F4DH::Core
{
	bool ParseViewPreferences(std::string_view a_json, ViewPreferences& a_inOut)
	{
		// Scan into a caller-seeded copy and commit only on full success, so
		// every failure mode (junk payload, malformed wrapper, no valid
		// field) leaves the caller's cached state untouched.
		ViewPreferences parsed = a_inOut;
		bool          anyField = false;
		if (!ScanObject(a_json, parsed, anyField) || !anyField) {
			return false;
		}
		a_inOut = parsed;
		return true;
	}

	std::string FormatViewPreferences(const ViewPreferences& a_preferences)
	{
		return std::format("{{\"fontSize\":{},\"pauseOnOpen\":{}}}",
			a_preferences.fontSize, a_preferences.pauseOnOpen);
	}
}
