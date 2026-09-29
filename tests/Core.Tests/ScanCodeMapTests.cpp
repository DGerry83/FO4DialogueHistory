#include <catch2/catch_test_macros.hpp>

#include "Core/Input/ScanCodeMap.h"

#include <string>

using F4DH::Core::ParseScanCode;

TEST_CASE("single letters resolve to their QWERTY DirectInput scan codes")
{
	REQUIRE(ParseScanCode("H") == 35);
	REQUIRE(ParseScanCode("h") == 35);
	REQUIRE(ParseScanCode("Q") == 16);
	REQUIRE(ParseScanCode("P") == 25);
	REQUIRE(ParseScanCode("A") == 30);
	REQUIRE(ParseScanCode("L") == 38);
	REQUIRE(ParseScanCode("Z") == 44);
	REQUIRE(ParseScanCode("M") == 50);
}

TEST_CASE("named keys resolve case-insensitively")
{
	REQUIRE(ParseScanCode("F2") == 60);
	REQUIRE(ParseScanCode("f12") == 88);
	REQUIRE(ParseScanCode("Space") == 57);
	REQUIRE(ParseScanCode("ENTER") == 28);
	REQUIRE(ParseScanCode("Escape") == 1);
	REQUIRE(ParseScanCode("esc") == 1);
	REQUIRE(ParseScanCode("Tab") == 15);
	REQUIRE(ParseScanCode("Backspace") == 14);
	REQUIRE(ParseScanCode("CapsLock") == 58);
}

TEST_CASE("modifier names map to left variants unless side is specified")
{
	REQUIRE(ParseScanCode("Shift") == 42);
	REQUIRE(ParseScanCode("LShift") == 42);
	REQUIRE(ParseScanCode("RShift") == 54);
	REQUIRE(ParseScanCode("Ctrl") == 29);
	REQUIRE(ParseScanCode("RCtrl") == 157);
	REQUIRE(ParseScanCode("Alt") == 56);
	REQUIRE(ParseScanCode("RAlt") == 184);
}

TEST_CASE("navigation and numpad names resolve")
{
	REQUIRE(ParseScanCode("Up") == 200);
	REQUIRE(ParseScanCode("ArrowDown") == 208);
	REQUIRE(ParseScanCode("Left") == 203);
	REQUIRE(ParseScanCode("Right") == 205);
	REQUIRE(ParseScanCode("Home") == 199);
	REQUIRE(ParseScanCode("End") == 207);
	REQUIRE(ParseScanCode("PgUp") == 201);
	REQUIRE(ParseScanCode("PageDown") == 209);
	REQUIRE(ParseScanCode("Insert") == 210);
	REQUIRE(ParseScanCode("Del") == 211);
	REQUIRE(ParseScanCode("Numpad0") == 82);
	REQUIRE(ParseScanCode("Numpad5") == 76);
	REQUIRE(ParseScanCode("NumPlus") == 78);
	REQUIRE(ParseScanCode("NumMinus") == 74);
	REQUIRE(ParseScanCode("NumMultiply") == 55);
	REQUIRE(ParseScanCode("NumDivide") == 181);
	REQUIRE(ParseScanCode("NumDecimal") == 83);
	REQUIRE(ParseScanCode("NumEnter") == 156);
	REQUIRE(ParseScanCode("NumLock") == 69);
}

TEST_CASE("punctuation names and symbol forms resolve")
{
	REQUIRE(ParseScanCode("Minus") == 12);
	REQUIRE(ParseScanCode("-") == 12);
	REQUIRE(ParseScanCode("Equals") == 13);
	REQUIRE(ParseScanCode("LBracket") == 26);
	REQUIRE(ParseScanCode("RBracket") == 27);
	REQUIRE(ParseScanCode("Semicolon") == 39);
	REQUIRE(ParseScanCode("Apostrophe") == 40);
	REQUIRE(ParseScanCode("Grave") == 41);
	REQUIRE(ParseScanCode("Backslash") == 43);
	REQUIRE(ParseScanCode("Comma") == 51);
	REQUIRE(ParseScanCode("Period") == 52);
	REQUIRE(ParseScanCode("Slash") == 53);
}

TEST_CASE("disable aliases map to zero")
{
	REQUIRE(ParseScanCode("off") == 0);
	REQUIRE(ParseScanCode("None") == 0);
	REQUIRE(ParseScanCode("DISABLED") == 0);
}

TEST_CASE("plain decimal values stay raw scan codes (original schema)")
{
	REQUIRE(ParseScanCode("35") == 35);
	REQUIRE(ParseScanCode("0") == 0);
	REQUIRE(ParseScanCode("255") == 255);
	// An all-digits value is NEVER a key name: "5" means DIK 5, not the 5 key.
	REQUIRE(ParseScanCode("5") == 5);
	// Out of DIK range is rejected rather than silently accepted.
	REQUIRE_FALSE(ParseScanCode("256").has_value());
	REQUIRE_FALSE(ParseScanCode("999999").has_value());
}

TEST_CASE("surrounding whitespace is ignored")
{
	REQUIRE(ParseScanCode("  H  ") == 35);
	REQUIRE(ParseScanCode("\tF2\t") == 60);
	REQUIRE(ParseScanCode(" 35 ") == 35);
}

TEST_CASE("garbage is rejected so the caller keeps its default")
{
	REQUIRE_FALSE(ParseScanCode("").has_value());
	REQUIRE_FALSE(ParseScanCode("   ").has_value());
	REQUIRE_FALSE(ParseScanCode("Banana").has_value());
	REQUIRE_FALSE(ParseScanCode("F13").has_value());
	REQUIRE_FALSE(ParseScanCode("HH").has_value());
	REQUIRE_FALSE(ParseScanCode("35a").has_value());
	REQUIRE_FALSE(ParseScanCode("-5").has_value());  // "-" alone is Minus, "-5" is junk
	REQUIRE_FALSE(ParseScanCode(std::string(64, 'x')).has_value());  // over-length
	REQUIRE_FALSE(ParseScanCode("H\x01").has_value());  // non-printable
}
