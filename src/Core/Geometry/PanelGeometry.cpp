#include "PanelGeometry.h"

#include <charconv>
#include <format>

namespace
{
	// Finds "key" : <integer> in the payload and converts it. Deliberately
	// small: the payload shape is fixed by our own view JS; tolerance means
	// never mis-accepting junk, not general JSON.
	[[nodiscard]] bool ParseIntField(std::string_view a_json, std::string_view a_key, int& a_out)
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

		const auto* const first = a_json.data() + i;
		const auto* const last  = a_json.data() + a_json.size();
		const auto        result = std::from_chars(first, last, a_out);
		if (result.ec != std::errc{} || result.ptr == first) {
			return false;
		}
		if (result.ptr != last) {
			const char next = *result.ptr;
			if (next != ',' && next != '}' && next != ' ' && next != '\t' && next != '\r' && next != '\n') {
				return false;  // value ran into non-JSON junk (e.g. "12abc", "1.5")
			}
		}
		return true;
	}
}

namespace F4DH::Core
{
	bool ParsePanelGeometry(std::string_view a_json, PanelGeometry& a_out)
	{
		PanelGeometry parsed;
		if (!ParseIntField(a_json, "x", parsed.x) ||
			!ParseIntField(a_json, "y", parsed.y) ||
			!ParseIntField(a_json, "width", parsed.width) ||
			!ParseIntField(a_json, "height", parsed.height)) {
			return false;
		}
		a_out = parsed;
		return true;
	}

	std::string FormatPanelGeometry(const PanelGeometry& a_geometry)
	{
		return std::format("{{\"x\":{},\"y\":{},\"width\":{},\"height\":{}}}",
			a_geometry.x, a_geometry.y, a_geometry.width, a_geometry.height);
	}
}
