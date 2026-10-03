#include "ViewFilterState.h"

#include <format>

namespace
{
	// Finds "key" : true|false in the payload and converts it. Deliberately
	// small: the payload shape is fixed by our own view JS; tolerance means
	// never mis-accepting junk, not general JSON.
	[[nodiscard]] bool ParseBoolField(std::string_view a_json, std::string_view a_key, bool& a_out)
	{
		const std::string quoted = std::format("\"{}\"", a_key);
		const auto        pos = a_json.find(quoted);
		if (pos == std::string_view::npos) {
			return false;
		}

		auto i = pos + quoted.size();
		const auto skipWs = [&a_json](std::size_t& a_index) {
			while (a_index < a_json.size()) {
				const char c = a_json[a_index];
				if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
					break;
				}
				a_index += 1;
			}
		};
		skipWs(i);
		if (i >= a_json.size() || a_json[i] != ':') {
			return false;
		}
		i += 1;
		skipWs(i);

		const auto matchLiteral = [&a_json](std::size_t a_index, std::string_view a_literal) {
			if (a_json.substr(a_index, a_literal.size()) != a_literal) {
				return false;
			}
			const auto end = a_index + a_literal.size();
			if (end != a_json.size()) {
				const char next = a_json[end];
				if (next != ',' && next != '}' && next != ' ' && next != '\t' && next != '\r' && next != '\n') {
					return false;  // literal ran into non-JSON junk (e.g. "trueX", "false1")
				}
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
	bool ParseViewFilterState(std::string_view a_json, ViewFilterState& a_out)
	{
		ViewFilterState parsed;
		bool            anyField = false;
		anyField = ParseBoolField(a_json, "main", parsed.main) || anyField;
		anyField = ParseBoolField(a_json, "side", parsed.side) || anyField;
		anyField = ParseBoolField(a_json, "misc", parsed.misc) || anyField;
		anyField = ParseBoolField(a_json, "unattributed", parsed.unattributed) || anyField;
		if (!anyField) {
			return false;
		}
		a_out = parsed;
		return true;
	}

	std::string FormatViewFilterState(const ViewFilterState& a_filters)
	{
		return std::format("{{\"main\":{},\"side\":{},\"misc\":{},\"unattributed\":{}}}",
			a_filters.main, a_filters.side, a_filters.misc, a_filters.unattributed);
	}
}
