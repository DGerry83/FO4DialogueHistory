#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace F4DH::Core
{
	// Panel geometry in view (viewport) pixel coordinates; x/y are the panel's
	// top-left corner. Engine-free payload type for the view's setGeometry /
	// geometryChanged messages (serialized as
	// {"x":N,"y":N,"width":N,"height":N}).
	struct PanelGeometry
	{
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
	};

	// M5-G1 minimum panel size; the view clamps to these on drag/resize and
	// the INI load path validates against them.
	inline constexpr int kPanelMinWidth{ 320 };
	inline constexpr int kPanelMinHeight{ 200 };

	// Tolerant parse of a geometry JSON object: the four integer fields, any
	// order, optional whitespace; a field value must end at a JSON boundary
	// (',' / '}' / whitespace). Missing fields, non-integer values, and
	// out-of-int ranges are rejected — the caller ignores such payloads.
	[[nodiscard]] bool ParsePanelGeometry(std::string_view a_json, PanelGeometry& a_out);

	[[nodiscard]] std::string FormatPanelGeometry(const PanelGeometry& a_geometry);
}
