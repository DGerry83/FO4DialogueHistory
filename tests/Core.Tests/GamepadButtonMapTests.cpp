#include <catch2/catch_test_macros.hpp>

#include "Core/Input/GamepadButtonMap.h"

#include <string>

using F4DH::Core::GamepadButtonName;
using F4DH::Core::kNoGamepadButton;
using F4DH::Core::ParseGamepadHotkey;

namespace
{
	// The 16 engine idCodes (XInput bit masks; LT/RT on F4SE's 0x09/0x0A).
	constexpr std::uint16_t kDUp{ 0x0001 };
	constexpr std::uint16_t kDDown{ 0x0002 };
	constexpr std::uint16_t kDLeft{ 0x0004 };
	constexpr std::uint16_t kDRight{ 0x0008 };
	constexpr std::uint16_t kStart{ 0x0010 };
	constexpr std::uint16_t kBack{ 0x0020 };
	constexpr std::uint16_t kLS{ 0x0040 };
	constexpr std::uint16_t kRS{ 0x0080 };
	constexpr std::uint16_t kLB{ 0x0100 };
	constexpr std::uint16_t kRB{ 0x0200 };
	constexpr std::uint16_t kA{ 0x1000 };
	constexpr std::uint16_t kB{ 0x2000 };
	constexpr std::uint16_t kX{ 0x4000 };
	constexpr std::uint16_t kY{ 0x8000 };
	constexpr std::uint16_t kLT{ 0x0009 };
	constexpr std::uint16_t kRT{ 0x000A };

	void RequireBinding(std::string_view a_value, std::uint16_t a_trigger,
		std::uint16_t a_modifier, bool a_enabled)
	{
		const auto binding = ParseGamepadHotkey(a_value);
		REQUIRE(binding.trigger == a_trigger);
		REQUIRE(binding.modifier == a_modifier);
		REQUIRE(binding.enabled == a_enabled);
	}
}

TEST_CASE("every canonical button name resolves to its engine idCode mask")
{
	RequireBinding("DUp", kDUp, kNoGamepadButton, true);
	RequireBinding("DDown", kDDown, kNoGamepadButton, true);
	RequireBinding("DLeft", kDLeft, kNoGamepadButton, true);
	RequireBinding("DRight", kDRight, kNoGamepadButton, true);
	RequireBinding("Start", kStart, kNoGamepadButton, true);
	RequireBinding("Back", kBack, kNoGamepadButton, true);
	RequireBinding("LS", kLS, kNoGamepadButton, true);
	RequireBinding("RS", kRS, kNoGamepadButton, true);
	RequireBinding("LB", kLB, kNoGamepadButton, true);
	RequireBinding("RB", kRB, kNoGamepadButton, true);
	RequireBinding("A", kA, kNoGamepadButton, true);
	RequireBinding("B", kB, kNoGamepadButton, true);
	RequireBinding("X", kX, kNoGamepadButton, true);
	RequireBinding("Y", kY, kNoGamepadButton, true);
	RequireBinding("LT", kLT, kNoGamepadButton, true);
	RequireBinding("RT", kRT, kNoGamepadButton, true);
}

TEST_CASE("button names resolve case-insensitively")
{
	RequireBinding("dup", kDUp, kNoGamepadButton, true);
	RequireBinding("ddOWN", kDDown, kNoGamepadButton, true);
	RequireBinding("START", kStart, kNoGamepadButton, true);
	RequireBinding("back", kBack, kNoGamepadButton, true);
	RequireBinding("lB", kLB, kNoGamepadButton, true);
	RequireBinding("rb", kRB, kNoGamepadButton, true);
	RequireBinding("a", kA, kNoGamepadButton, true);
	RequireBinding("Rt", kRT, kNoGamepadButton, true);
}

TEST_CASE("gamepad binding whitespace is ignored")
{
	RequireBinding("  DUp  ", kDUp, kNoGamepadButton, true);
	RequireBinding("\tLB\t", kLB, kNoGamepadButton, true);
	RequireBinding(" 256 ", kLB, kNoGamepadButton, true);
}

TEST_CASE("chords bind modifier and trigger")
{
	RequireBinding("LB+DUp", kDUp, kLB, true);
	RequireBinding("Back+Y", kY, kBack, true);
	RequireBinding("RB+X", kX, kRB, true);
	RequireBinding("LT+RT", kRT, kLT, true);
}

TEST_CASE("chord parts resolve case-insensitively with tolerated whitespace")
{
	RequireBinding("lb+dup", kDUp, kLB, true);
	RequireBinding(" LB + DUp ", kDUp, kLB, true);
	RequireBinding("\tBack+\tY\t", kY, kBack, true);
}

TEST_CASE("plain decimals are accepted only when they equal a known button mask")
{
	RequireBinding("1", kDUp, kNoGamepadButton, true);
	RequireBinding("9", kLT, kNoGamepadButton, true);    // F4SE trigger code
	RequireBinding("10", kRT, kNoGamepadButton, true);   // F4SE trigger code
	RequireBinding("16", kStart, kNoGamepadButton, true);
	RequireBinding("256", kLB, kNoGamepadButton, true);
	RequireBinding("32768", kY, kNoGamepadButton, true);
	// Numbers that match no known mask are rejected, not silently accepted.
	RequireBinding("0", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("3", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("5", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("15", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("17", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("255", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("999999", kNoGamepadButton, kNoGamepadButton, false);
}

TEST_CASE("chords reject duplicate buttons")
{
	RequireBinding("LB+LB", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("lb+LB", kNoGamepadButton, kNoGamepadButton, false);  // duplicate is duplicate case-insensitively
	RequireBinding("A+a", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("DUp+DUp", kNoGamepadButton, kNoGamepadButton, false);
}

TEST_CASE("chords reject malformed shapes")
{
	RequireBinding("LB+", kNoGamepadButton, kNoGamepadButton, false);       // empty trigger part
	RequireBinding("+DUp", kNoGamepadButton, kNoGamepadButton, false);      // empty modifier part
	RequireBinding("+", kNoGamepadButton, kNoGamepadButton, false);         // both parts empty
	RequireBinding("LB++DUp", kNoGamepadButton, kNoGamepadButton, false);   // too many parts
	RequireBinding("LB+DUp+DDown", kNoGamepadButton, kNoGamepadButton, false);  // too many parts
	RequireBinding("LB+Banana", kNoGamepadButton, kNoGamepadButton, false); // unknown trigger
	RequireBinding("Banana+DUp", kNoGamepadButton, kNoGamepadButton, false);// unknown modifier
}

TEST_CASE("chords reject numeric parts (numeric schema is whole-value only)")
{
	RequireBinding("LB+256", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("256+LB", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("256+1", kNoGamepadButton, kNoGamepadButton, false);
}

TEST_CASE("garbage is rejected so the caller keeps the feature off")
{
	RequireBinding("", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("   ", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("\t\t", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("Banana", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("F13", kNoGamepadButton, kNoGamepadButton, false);     // a keyboard name, not a button
	RequireBinding("DUp5", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("256a", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("-256", kNoGamepadButton, kNoGamepadButton, false);
	RequireBinding("0x100", kNoGamepadButton, kNoGamepadButton, false);   // decimal only
	RequireBinding("DUp\x01", kNoGamepadButton, kNoGamepadButton, false);  // non-printable junk
	RequireBinding(std::string(64, 'x'), kNoGamepadButton, kNoGamepadButton, false);  // over-length
}

TEST_CASE("GamepadButtonName returns the canonical name for every code")
{
	REQUIRE(GamepadButtonName(kDUp) == "DUp");
	REQUIRE(GamepadButtonName(kDDown) == "DDown");
	REQUIRE(GamepadButtonName(kDLeft) == "DLeft");
	REQUIRE(GamepadButtonName(kDRight) == "DRight");
	REQUIRE(GamepadButtonName(kStart) == "Start");
	REQUIRE(GamepadButtonName(kBack) == "Back");
	REQUIRE(GamepadButtonName(kLS) == "LS");
	REQUIRE(GamepadButtonName(kRS) == "RS");
	REQUIRE(GamepadButtonName(kLB) == "LB");
	REQUIRE(GamepadButtonName(kRB) == "RB");
	REQUIRE(GamepadButtonName(kA) == "A");
	REQUIRE(GamepadButtonName(kB) == "B");
	REQUIRE(GamepadButtonName(kX) == "X");
	REQUIRE(GamepadButtonName(kY) == "Y");
	REQUIRE(GamepadButtonName(kLT) == "LT");
	REQUIRE(GamepadButtonName(kRT) == "RT");
}

TEST_CASE("GamepadButtonName round-trips through the parser")
{
	constexpr std::uint16_t kCodes[]{
		kDUp, kDDown, kDLeft, kDRight, kStart, kBack, kLS, kRS,
		kLB, kRB, kA, kB, kX, kY, kLT, kRT,
	};
	for (const auto code : kCodes) {
		const auto name = GamepadButtonName(code);
		REQUIRE_FALSE(name.empty());
		REQUIRE(name != "?");
		RequireBinding(name, code, kNoGamepadButton, true);
	}
}

TEST_CASE("GamepadButtonName returns ? for unknown codes")
{
	REQUIRE(GamepadButtonName(0) == "?");
	REQUIRE(GamepadButtonName(0x0003) == "?");
	REQUIRE(GamepadButtonName(0x0400) == "?");
	REQUIRE(GamepadButtonName(0xFFFF) == "?");
}
