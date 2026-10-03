#pragma once

#include <string>
#include <string_view>

namespace F4DH::Core
{
	// Quest-type bucket filter toggles, one bool per view bucket. Defaults are
	// all-true (every bucket shown) — the absent-persistence state. Engine-free
	// payload type for the view's setFilters / filtersChanged messages
	// (serialized as {"main":bool,"side":bool,"misc":bool,"unattributed":bool}).
	struct ViewFilterState
	{
		bool main = true;
		bool side = true;
		bool misc = true;
		bool unattributed = true;
	};

	// Tolerant parse of a filters JSON object, per field: each key is looked
	// up independently and must be a JSON true/false literal ending at a JSON
	// boundary (',' / '}' / whitespace); a missing or invalid field keeps its
	// default (true) instead of failing the whole payload. Returns false only
	// when no field parsed (empty or junk payload) — the caller drops such
	// payloads without persisting. Extra keys are ignored.
	[[nodiscard]] bool ParseViewFilterState(std::string_view a_json, ViewFilterState& a_out);

	[[nodiscard]] std::string FormatViewFilterState(const ViewFilterState& a_filters);
}
