#include <catch2/catch_test_macros.hpp>

#include "Core/Settings/ViewFilterState.h"

TEST_CASE("view filter state defaults to all buckets shown")
{
	const F4DH::Core::ViewFilterState filters;
	REQUIRE(filters.main);
	REQUIRE(filters.side);
	REQUIRE(filters.misc);
	REQUIRE(filters.unattributed);
}

TEST_CASE("view filter state round-trips through format and parse")
{
	const F4DH::Core::ViewFilterState filters{ false, true, false, true };
	const auto                        json = F4DH::Core::FormatViewFilterState(filters);
	REQUIRE(json == "{\"main\":false,\"side\":true,\"misc\":false,\"unattributed\":true}");

	F4DH::Core::ViewFilterState parsed;
	REQUIRE(F4DH::Core::ParseViewFilterState(json, parsed));
	REQUIRE(!parsed.main);
	REQUIRE(parsed.side);
	REQUIRE(!parsed.misc);
	REQUIRE(parsed.unattributed);
}

TEST_CASE("view filter state parse accepts fields in any order with surrounding whitespace")
{
	F4DH::Core::ViewFilterState parsed;
	REQUIRE(F4DH::Core::ParseViewFilterState(
		"{ \"unattributed\" : false ,\t\"misc\"\r\n:\ntrue , \"side\" : false , \"main\" : true }", parsed));
	REQUIRE(parsed.main);
	REQUIRE(!parsed.side);
	REQUIRE(parsed.misc);
	REQUIRE(!parsed.unattributed);
}

TEST_CASE("view filter state parse fills missing fields with defaults")
{
	F4DH::Core::ViewFilterState parsed;
	REQUIRE(F4DH::Core::ParseViewFilterState("{\"misc\":false}", parsed));
	REQUIRE(parsed.main);
	REQUIRE(parsed.side);
	REQUIRE(!parsed.misc);
	REQUIRE(parsed.unattributed);
}

TEST_CASE("view filter state parse fills invalid field values with defaults but keeps valid fields")
{
	F4DH::Core::ViewFilterState parsed;
	REQUIRE(F4DH::Core::ParseViewFilterState(
		"{\"main\":false,\"side\":1,\"misc\":true,\"unattributed\":no}", parsed));
	REQUIRE(!parsed.main);   // valid literal kept
	REQUIRE(parsed.side);    // numeric 1 is not a JSON bool literal -> default
	REQUIRE(parsed.misc);    // valid literal kept
	REQUIRE(parsed.unattributed);  // bare word -> default
}

TEST_CASE("view filter state parse rejects junk payloads without touching the output")
{
	F4DH::Core::ViewFilterState parsed{ false, false, false, false };
	REQUIRE(!F4DH::Core::ParseViewFilterState("", parsed));
	REQUIRE(!F4DH::Core::ParseViewFilterState("not json", parsed));
	REQUIRE(!F4DH::Core::ParseViewFilterState("{}", parsed));
	REQUIRE(!F4DH::Core::ParseViewFilterState("{\"foo\":1}", parsed));
	REQUIRE(!F4DH::Core::ParseViewFilterState("{\"main\":1}", parsed));
	// failed parses leave the caller's state untouched
	REQUIRE(!parsed.main);
	REQUIRE(!parsed.side);
	REQUIRE(!parsed.misc);
	REQUIRE(!parsed.unattributed);
}

TEST_CASE("view filter state parse ignores extra fields")
{
	F4DH::Core::ViewFilterState parsed;
	REQUIRE(F4DH::Core::ParseViewFilterState(
		"{\"main\":false,\"extra\":1,\"note\":\"x\",\"unattributed\":false}", parsed));
	REQUIRE(!parsed.main);
	REQUIRE(parsed.side);
	REQUIRE(parsed.misc);
	REQUIRE(!parsed.unattributed);
}

TEST_CASE("view filter state parse rejects bool literals that run into junk")
{
	F4DH::Core::ViewFilterState parsed;
	REQUIRE(!F4DH::Core::ParseViewFilterState("{\"main\":trueX}", parsed));
	REQUIRE(!F4DH::Core::ParseViewFilterState("{\"main\":false1}", parsed));
	REQUIRE(F4DH::Core::ParseViewFilterState("{\"main\":true}", parsed));
	REQUIRE(parsed.main);
}
