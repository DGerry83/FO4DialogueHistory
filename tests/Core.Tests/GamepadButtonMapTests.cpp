#include <catch2/catch_test_macros.hpp>

#include "Core/Input/GamepadButtonMap.h"

#include <string>

using F4DH::Core::GamepadButtonName;
using F4DH::Core::ParseGamepadHotkey;

namespace
{
	constexpr std::uint8_t kNoModifier{ 0xFF };

	void RequireBinding(std::string_view a_value, std::uint8_t a_trigger,
		std::uint8_t a_modifier, bool a_enabled)
	{
		const auto binding = ParseGamepadHotkey(a_value);
		REQUIRE(binding.trigger == a_trigger);
		REQUIRE(binding.modifier == a_modifier);
		REQUIRE(binding.enabled == a_enabled);
	}
}

TEST_CASE("every canonical button name resolves to its device-local code")
{
	RequireBinding("DUp", 0, kNoModifier, true);
	RequireBinding("DDown", 1, kNoModifier, true);
	RequireBinding("DLeft", 2, kNoModifier, true);
	RequireBinding("DRight", 3, kNoModifier, true);
	RequireBinding("Start", 4, kNoModifier, true);
	RequireBinding("Back", 5, kNoModifier, true);
	RequireBinding("LS", 6, kNoModifier, true);
	RequireBinding("RS", 7, kNoModifier, true);
	RequireBinding("LB", 8, kNoModifier, true);
	RequireBinding("RB", 9, kNoModifier, true);
	RequireBinding("A", 10, kNoModifier, true);
	RequireBinding("B", 11, kNoModifier, true);
	RequireBinding("X", 12, kNoModifier, true);
	RequireBinding("Y", 13, kNoModifier, true);
	RequireBinding("LT", 14, kNoModifier, true);
	RequireBinding("RT", 15, kNoModifier, true);
}

TEST_CASE("button names resolve case-insensitively")
{
	RequireBinding("dup", 0, kNoModifier, true);
	RequireBinding("ddOWN", 1, kNoModifier, true);
	RequireBinding("START", 4, kNoModifier, true);
	RequireBinding("back", 5, kNoModifier, true);
	RequireBinding("lB", 8, kNoModifier, true);
	RequireBinding("rb", 9, kNoModifier, true);
	RequireBinding("a", 10, kNoModifier, true);
	RequireBinding("Rt", 15, kNoModifier, true);
}

TEST_CASE("gamepad binding whitespace is ignored")
{
	RequireBinding("  DUp  ", 0, kNoModifier, true);
	RequireBinding("\tLB\t", 8, kNoModifier, true);
	RequireBinding(" 15 ", 15, kNoModifier, true);
}

TEST_CASE("chords bind modifier and trigger")
{
	RequireBinding("LB+DUp", 0, 8, true);
	RequireBinding("Back+Y", 13, 5, true);
	RequireBinding("RB+X", 12, 9, true);
	RequireBinding("LT+RT", 15, 14, true);
}

TEST_CASE("chord parts resolve case-insensitively with tolerated whitespace")
{
	RequireBinding("lb+dup", 0, 8, true);
	RequireBinding(" LB + DUp ", 0, 8, true);
	RequireBinding("\tBack+\tY\t", 13, 5, true);
}

TEST_CASE("plain decimal values 0-15 stay raw button codes (original schema)")
{
	RequireBinding("0", 0, kNoModifier, true);
	RequireBinding("5", 5, kNoModifier, true);
	RequireBinding("15", 15, kNoModifier, true);
	// Out of the 0-15 button range is rejected rather than silently accepted.
	RequireBinding("16", 0, kNoModifier, false);
	RequireBinding("255", 0, kNoModifier, false);
	RequireBinding("999999", 0, kNoModifier, false);
}

TEST_CASE("chords reject duplicate buttons")
{
	RequireBinding("LB+LB", 0, kNoModifier, false);
	RequireBinding("lb+LB", 0, kNoModifier, false);  // duplicate is duplicate case-insensitively
	RequireBinding("A+a", 0, kNoModifier, false);
	RequireBinding("DUp+DUp", 0, kNoModifier, false);
}

TEST_CASE("chords reject malformed shapes")
{
	RequireBinding("LB+", 0, kNoModifier, false);       // empty trigger part
	RequireBinding("+DUp", 0, kNoModifier, false);      // empty modifier part
	RequireBinding("+", 0, kNoModifier, false);         // both parts empty
	RequireBinding("LB++DUp", 0, kNoModifier, false);   // too many parts
	RequireBinding("LB+DUp+DDown", 0, kNoModifier, false);  // too many parts
	RequireBinding("LB+Banana", 0, kNoModifier, false); // unknown trigger
	RequireBinding("Banana+DUp", 0, kNoModifier, false);// unknown modifier
}

TEST_CASE("chords reject numeric parts (numeric schema is whole-value only)")
{
	RequireBinding("LB+5", 0, kNoModifier, false);
	RequireBinding("5+LB", 0, kNoModifier, false);
	RequireBinding("5+6", 0, kNoModifier, false);
}

TEST_CASE("garbage is rejected so the caller keeps the feature off")
{
	RequireBinding("", 0, kNoModifier, false);
	RequireBinding("   ", 0, kNoModifier, false);
	RequireBinding("\t\t", 0, kNoModifier, false);
	RequireBinding("Banana", 0, kNoModifier, false);
	RequireBinding("F13", 0, kNoModifier, false);     // a keyboard name, not a button
	RequireBinding("DUp5", 0, kNoModifier, false);
	RequireBinding("15a", 0, kNoModifier, false);
	RequireBinding("-5", 0, kNoModifier, false);
	RequireBinding("DUp\x01", 0, kNoModifier, false);  // non-printable junk
	RequireBinding(std::string(64, 'x'), 0, kNoModifier, false);  // over-length
}

TEST_CASE("GamepadButtonName returns the canonical name for every code")
{
	REQUIRE(GamepadButtonName(0) == "DUp");
	REQUIRE(GamepadButtonName(1) == "DDown");
	REQUIRE(GamepadButtonName(2) == "DLeft");
	REQUIRE(GamepadButtonName(3) == "DRight");
	REQUIRE(GamepadButtonName(4) == "Start");
	REQUIRE(GamepadButtonName(5) == "Back");
	REQUIRE(GamepadButtonName(6) == "LS");
	REQUIRE(GamepadButtonName(7) == "RS");
	REQUIRE(GamepadButtonName(8) == "LB");
	REQUIRE(GamepadButtonName(9) == "RB");
	REQUIRE(GamepadButtonName(10) == "A");
	REQUIRE(GamepadButtonName(11) == "B");
	REQUIRE(GamepadButtonName(12) == "X");
	REQUIRE(GamepadButtonName(13) == "Y");
	REQUIRE(GamepadButtonName(14) == "LT");
	REQUIRE(GamepadButtonName(15) == "RT");
}

TEST_CASE("GamepadButtonName round-trips through the parser")
{
	for (std::uint8_t code = 0; code < 16; ++code) {
		const auto name = GamepadButtonName(code);
		REQUIRE_FALSE(name.empty());
		REQUIRE(name != "?");
		RequireBinding(name, code, kNoModifier, true);
	}
}

TEST_CASE("GamepadButtonName returns ? for out-of-range codes")
{
	REQUIRE(GamepadButtonName(16) == "?");
	REQUIRE(GamepadButtonName(0xFF) == "?");
}
