#pragma once

#include <string>
#include <string_view>

namespace F4DH::Core
{
	// View presentation preferences: panel-wide font size (integer px, 10–48,
	// default 16) and whether opening the panel pauses the game (default
	// false = unpaused). Engine-free payload type for the view's
	// setViewPreferences / viewPreferencesChanged messages
	// (serialized as {"fontSize":int,"pauseOnOpen":bool}).
	struct ViewPreferences
	{
		int  fontSize = 16;
		bool pauseOnOpen = false;
	};

	// Tolerant parse of a preferences JSON object into a_inOut, which the
	// caller seeds with the current cached state: each key is looked up
	// independently; fontSize must be a JSON integer in 10–48 and pauseOnOpen
	// a JSON true/false literal ending at a JSON boundary (',' / '}' /
	// whitespace). A missing or invalid field keeps the seeded value. Returns
	// false and leaves a_inOut untouched when no field parsed (empty or junk
	// payload) — the caller drops such payloads without caching or
	// persisting. Extra keys are ignored.
	[[nodiscard]] bool ParseViewPreferences(std::string_view a_json, ViewPreferences& a_inOut);

	[[nodiscard]] std::string FormatViewPreferences(const ViewPreferences& a_preferences);
}
