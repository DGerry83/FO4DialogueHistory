#include <catch2/catch_test_macros.hpp>

#include "Core/Dialogue/AliasTokens.h"

#include <optional>
#include <string>
#include <string_view>

namespace
{
	// Lookup backed by a single name/value pair; anything else misses.
	auto SinglePairLookup(std::string_view name, std::string_view value)
	{
		return [name, value](std::string_view query) -> std::optional<std::string> {
			if (query == name) {
				return std::string(value);
			}
			return std::nullopt;
		};
	}

	auto AlwaysMiss()
	{
		return [](std::string_view) -> std::optional<std::string> { return std::nullopt; };
	}
}

TEST_CASE("substitution resolves a single token")
{
	const auto out = F4DH::Core::AliasTokens::Substitute(
		"deal with the raiders at <alias=targetname>",
		SinglePairLookup("targetname", "Corvega Assembly Plant"));
	REQUIRE(out == "deal with the raiders at Corvega Assembly Plant");
}

TEST_CASE("unresolved token stays verbatim")
{
	const std::string_view raw = "deal with the raiders at <alias=targetname>";
	const auto out = F4DH::Core::AliasTokens::Substitute(raw, AlwaysMiss());
	REQUIRE(out == raw);
}

TEST_CASE("malformed token without closing bracket stays verbatim")
{
	const std::string_view raw = "deal with the raiders at <alias=targetname";
	const auto out = F4DH::Core::AliasTokens::Substitute(raw, SinglePairLookup("targetname", "X"));
	REQUIRE(out == raw);
}

TEST_CASE("multiple tokens resolve independently")
{
	const auto out = F4DH::Core::AliasTokens::Substitute(
		"<alias=giver> wants <alias=targetname> cleared",
		[](std::string_view q) -> std::optional<std::string> {
			if (q == "giver") return std::string("Preston");
			if (q == "targetname") return std::string("the Corvega plant");
			return std::nullopt;
		});
	REQUIRE(out == "Preston wants the Corvega plant cleared");
}

TEST_CASE("property suffix resolves by the base alias name")
{
	const auto out = F4DH::Core::AliasTokens::Substitute(
		"<alias=boss.ShortName> must die",
		SinglePairLookup("boss", "Jared"));
	REQUIRE(out == "Jared must die");
}

TEST_CASE("keyword match is case-insensitive")
{
	const auto out = F4DH::Core::AliasTokens::Substitute(
		"<ALIAS=targetname> awaits",
		SinglePairLookup("targetname", "Jared"));
	REQUIRE(out == "Jared awaits");
}

TEST_CASE("text without tokens is unchanged")
{
	const std::string_view raw = "Rescue the settler <not-a-token>";
	const auto out = F4DH::Core::AliasTokens::Substitute(raw, AlwaysMiss());
	REQUIRE(out == raw);
}

TEST_CASE("empty alias name stays verbatim")
{
	const std::string_view raw = "go to <alias=> now";
	const auto out = F4DH::Core::AliasTokens::Substitute(raw, SinglePairLookup("", "X"));
	REQUIRE(out == raw);
}

TEST_CASE("ContainsToken detects tokens case-insensitively")
{
	REQUIRE(F4DH::Core::AliasTokens::ContainsToken("x <alias=a> y"));
	REQUIRE(F4DH::Core::AliasTokens::ContainsToken("x <Alias=a>"));
	REQUIRE_FALSE(F4DH::Core::AliasTokens::ContainsToken("plain text"));
	REQUIRE_FALSE(F4DH::Core::AliasTokens::ContainsToken("<aliases=a>"));
}
