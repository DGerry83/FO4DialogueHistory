#include <catch2/catch_test_macros.hpp>

#include "Core/Geometry/PanelGeometry.h"

TEST_CASE("panel geometry round-trips through format and parse")
{
	const F4DH::Core::PanelGeometry geometry{ 100, 240, 800, 600 };
	const auto                        json = F4DH::Core::FormatPanelGeometry(geometry);
	REQUIRE(json == "{\"x\":100,\"y\":240,\"width\":800,\"height\":600}");

	F4DH::Core::PanelGeometry parsed;
	REQUIRE(F4DH::Core::ParsePanelGeometry(json, parsed));
	REQUIRE(parsed.x == 100);
	REQUIRE(parsed.y == 240);
	REQUIRE(parsed.width == 800);
	REQUIRE(parsed.height == 600);
}

TEST_CASE("parse accepts fields in any order with surrounding whitespace")
{
	F4DH::Core::PanelGeometry parsed;
	REQUIRE(F4DH::Core::ParsePanelGeometry(
		"{ \"height\" : 600 ,\t\"width\"\r\n:\n800 , \"y\" : 240 , \"x\" : 100 }", parsed));
	REQUIRE(parsed.x == 100);
	REQUIRE(parsed.y == 240);
	REQUIRE(parsed.width == 800);
	REQUIRE(parsed.height == 600);
}

TEST_CASE("parse accepts negative coordinates")
{
	F4DH::Core::PanelGeometry parsed;
	REQUIRE(F4DH::Core::ParsePanelGeometry("{\"x\":-10,\"y\":-20,\"width\":800,\"height\":600}", parsed));
	REQUIRE(parsed.x == -10);
	REQUIRE(parsed.y == -20);
}

TEST_CASE("parse rejects missing or non-integer fields")
{
	F4DH::Core::PanelGeometry parsed;
	REQUIRE(!F4DH::Core::ParsePanelGeometry("", parsed));
	REQUIRE(!F4DH::Core::ParsePanelGeometry("not json", parsed));
	REQUIRE(!F4DH::Core::ParsePanelGeometry("{}", parsed));
	REQUIRE(!F4DH::Core::ParsePanelGeometry("{\"x\":1,\"y\":2,\"width\":3}", parsed));
	REQUIRE(!F4DH::Core::ParsePanelGeometry("{\"x\":1.5,\"y\":2,\"width\":3,\"height\":4}", parsed));
	REQUIRE(!F4DH::Core::ParsePanelGeometry("{\"x\":12abc,\"y\":2,\"width\":3,\"height\":4}", parsed));
	REQUIRE(!F4DH::Core::ParsePanelGeometry("{\"x\":\"1\",\"y\":2,\"width\":3,\"height\":4}", parsed));
}

TEST_CASE("parse rejects out-of-range integers")
{
	F4DH::Core::PanelGeometry parsed;
	REQUIRE(!F4DH::Core::ParsePanelGeometry("{\"x\":2147483648,\"y\":0,\"width\":800,\"height\":600}", parsed));
	REQUIRE(!F4DH::Core::ParsePanelGeometry("{\"x\":99999999999999999999,\"y\":0,\"width\":800,\"height\":600}", parsed));
}

TEST_CASE("minimum panel size constants match the M5 clamp")
{
	REQUIRE(F4DH::Core::kPanelMinWidth == 320);
	REQUIRE(F4DH::Core::kPanelMinHeight == 200);
}
