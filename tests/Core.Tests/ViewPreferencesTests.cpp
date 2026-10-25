#include <catch2/catch_test_macros.hpp>

#include "Core/Settings/ViewPreferences.h"

TEST_CASE("view preferences default to 16 px and unpaused")
{
	const F4DH::Core::ViewPreferences prefs;
	REQUIRE(prefs.fontSize == 16);
	REQUIRE(!prefs.pauseOnOpen);
}

TEST_CASE("view preferences round-trip through format and parse")
{
	const F4DH::Core::ViewPreferences prefs{ 24, true };
	const auto                       json = F4DH::Core::FormatViewPreferences(prefs);
	REQUIRE(json == "{\"fontSize\":24,\"pauseOnOpen\":true}");

	F4DH::Core::ViewPreferences parsed;
	REQUIRE(F4DH::Core::ParseViewPreferences(json, parsed));
	REQUIRE(parsed.fontSize == 24);
	REQUIRE(parsed.pauseOnOpen);
}

TEST_CASE("view preferences parse accepts both pause states")
{
	F4DH::Core::ViewPreferences parsed;
	REQUIRE(F4DH::Core::ParseViewPreferences("{\"fontSize\":20,\"pauseOnOpen\":false}", parsed));
	REQUIRE(parsed.fontSize == 20);
	REQUIRE(!parsed.pauseOnOpen);

	REQUIRE(F4DH::Core::ParseViewPreferences("{\"fontSize\":20,\"pauseOnOpen\":true}", parsed));
	REQUIRE(parsed.fontSize == 20);
	REQUIRE(parsed.pauseOnOpen);
}

TEST_CASE("view preferences parse accepts the integer font boundaries 10 and 48")
{
	F4DH::Core::ViewPreferences parsed;
	REQUIRE(F4DH::Core::ParseViewPreferences("{\"fontSize\":10}", parsed));
	REQUIRE(parsed.fontSize == 10);

	REQUIRE(F4DH::Core::ParseViewPreferences("{\"fontSize\":48}", parsed));
	REQUIRE(parsed.fontSize == 48);
}

TEST_CASE("view preferences parse rejects out-of-range font sizes without touching cached state")
{
	F4DH::Core::ViewPreferences parsed{ 99, true };
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":0}", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":1}", parsed));    // legacy lower bound
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":9}", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":49}", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":128}", parsed));  // legacy upper bound
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":129}", parsed));
	REQUIRE(parsed.fontSize == 99);   // rejected payloads never replace cached state
	REQUIRE(parsed.pauseOnOpen);      // untouched
}

TEST_CASE("view preferences parse rejects fractional and non-numeric font sizes")
{
	F4DH::Core::ViewPreferences parsed{ 42, false };
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":16.5}", parsed));
	REQUIRE(parsed.fontSize == 42);
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":abc}", parsed));
	REQUIRE(parsed.fontSize == 42);
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":16x}", parsed));
	REQUIRE(parsed.fontSize == 42);
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":-8}", parsed));
	REQUIRE(parsed.fontSize == 42);
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":1e2}", parsed));
	REQUIRE(parsed.fontSize == 42);
}

TEST_CASE("view preferences parse treats each field independently on the seeded state")
{
	F4DH::Core::ViewPreferences parsed{ 88, true };

	// fontSize only: pause keeps its seeded value
	REQUIRE(F4DH::Core::ParseViewPreferences("{\"fontSize\":32}", parsed));
	REQUIRE(parsed.fontSize == 32);
	REQUIRE(parsed.pauseOnOpen);

	// pauseOnOpen only: font keeps its current (previously parsed) value
	REQUIRE(F4DH::Core::ParseViewPreferences("{\"pauseOnOpen\":false}", parsed));
	REQUIRE(parsed.fontSize == 32);
	REQUIRE(!parsed.pauseOnOpen);

	// one valid + one invalid field: the valid one applies
	REQUIRE(F4DH::Core::ParseViewPreferences("{\"fontSize\":999,\"pauseOnOpen\":true}", parsed));
	REQUIRE(parsed.fontSize == 32);
	REQUIRE(parsed.pauseOnOpen);
}

TEST_CASE("view preferences parse accepts fields in any order with surrounding whitespace")
{
	F4DH::Core::ViewPreferences parsed;
	REQUIRE(F4DH::Core::ParseViewPreferences(
		"{ \"pauseOnOpen\" : true ,\t\"fontSize\"\r\n:\n12 }", parsed));
	REQUIRE(parsed.fontSize == 12);
	REQUIRE(parsed.pauseOnOpen);
}

TEST_CASE("view preferences parse ignores extra fields")
{
	F4DH::Core::ViewPreferences parsed;
	REQUIRE(F4DH::Core::ParseViewPreferences(
		"{\"fontSize\":20,\"extra\":1,\"note\":\"x\",\"pauseOnOpen\":true}", parsed));
	REQUIRE(parsed.fontSize == 20);
	REQUIRE(parsed.pauseOnOpen);
}

TEST_CASE("view preferences parse rejects junk payloads without touching the output")
{
	F4DH::Core::ViewPreferences parsed{ 77, true };
	REQUIRE(!F4DH::Core::ParseViewPreferences("", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("not json", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("{}", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("[]", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"foo\":1}", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"pauseOnOpen\":1}", parsed));
	// failed parses leave the caller's cached state untouched
	REQUIRE(parsed.fontSize == 77);
	REQUIRE(parsed.pauseOnOpen);
}

TEST_CASE("view preferences parse rejects malformed wrappers that quote member keys", "[structural]")
{
	F4DH::Core::ViewPreferences parsed{ 99, true };
	REQUIRE(!F4DH::Core::ParseViewPreferences("not json \"fontSize\":128", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("garbage \"pauseOnOpen\":true", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("\"fontSize\":128", parsed));      // bare member, no object wrapper
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":128", parsed));     // unterminated object
	REQUIRE(parsed.fontSize == 99);   // wholesale rejection: cached state untouched
	REQUIRE(parsed.pauseOnOpen);
}

TEST_CASE("view preferences parse ignores quoted key lookalikes inside string values", "[structural]")
{
	F4DH::Core::ViewPreferences parsed{ 16, false };
	// audit example: the escaped lookalike inside "note" must not shadow the real member
	REQUIRE(F4DH::Core::ParseViewPreferences("{\"note\":\"\\\"fontSize\\\":999\",\"fontSize\":20}", parsed));
	REQUIRE(parsed.fontSize == 20);
	REQUIRE(!parsed.pauseOnOpen);
	// in-range lookalike would have hijacked the value under substring search
	parsed = { 16, false };
	REQUIRE(F4DH::Core::ParseViewPreferences("{\"note\":\"\\\"fontSize\\\":50\",\"fontSize\":20}", parsed));
	REQUIRE(parsed.fontSize == 20);
}

TEST_CASE("view preferences parse ignores key lookalikes inside nested objects and arrays", "[structural]")
{
	F4DH::Core::ViewPreferences parsed{ 16, false };
	REQUIRE(F4DH::Core::ParseViewPreferences("{\"extra\":{\"fontSize\":50},\"fontSize\":20}", parsed));
	REQUIRE(parsed.fontSize == 20);   // nested-object lookalike does not shadow
	parsed = { 16, false };
	REQUIRE(F4DH::Core::ParseViewPreferences("{\"extra\":[\"pauseOnOpen\",true],\"pauseOnOpen\":false}", parsed));
	REQUIRE(!parsed.pauseOnOpen);     // array lookalike does not shadow
	REQUIRE(parsed.fontSize == 16);
}

TEST_CASE("view preferences parse rejects trailing junk after the closing brace", "[structural]")
{
	F4DH::Core::ViewPreferences parsed{ 55, true };
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":20} garbage", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":20,\"pauseOnOpen\":true} x", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":20}}", parsed));
	REQUIRE(parsed.fontSize == 55);   // a valid-looking field never legitimizes junk
	REQUIRE(parsed.pauseOnOpen);
}

TEST_CASE("view preferences parse still accepts a complete valid object with extra fields", "[structural]")
{
	F4DH::Core::ViewPreferences parsed{ 16, false };
	REQUIRE(F4DH::Core::ParseViewPreferences(
		"{\"fontSize\":28,\"extra\":{\"deep\":[1,2,{\"pauseOnOpen\":true}]},\"note\":\"text\",\"pauseOnOpen\":true}", parsed));
	REQUIRE(parsed.fontSize == 28);
	REQUIRE(parsed.pauseOnOpen);
}

TEST_CASE("view preferences parse rejects mismatched nested brackets", "[structural]")
{
	F4DH::Core::ViewPreferences parsed{ 66, true };
	// the '}' does not close '[': the malformed extra value must fail wholesale
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"extra\":[}],\"fontSize\":20}", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"extra\":[{]},\"fontSize\":20}", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"extra\":{\"a\":1],\"fontSize\":20}", parsed));
	REQUIRE(parsed.fontSize == 66);   // cached state untouched
	REQUIRE(parsed.pauseOnOpen);
}

TEST_CASE("view preferences parse rejects trailing commas", "[structural]")
{
	F4DH::Core::ViewPreferences parsed{ 44, true };
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":20,}", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":20, }", parsed));
	REQUIRE(!F4DH::Core::ParseViewPreferences("{\"fontSize\":20,\"pauseOnOpen\":true,}", parsed));
	REQUIRE(parsed.fontSize == 44);   // cached state untouched
	REQUIRE(parsed.pauseOnOpen);
}

TEST_CASE("view preferences format emits integer font and JSON bool literal")
{
	REQUIRE(F4DH::Core::FormatViewPreferences({ 10, false }) == "{\"fontSize\":10,\"pauseOnOpen\":false}");
	REQUIRE(F4DH::Core::FormatViewPreferences({ 48, true }) == "{\"fontSize\":48,\"pauseOnOpen\":true}");
}
