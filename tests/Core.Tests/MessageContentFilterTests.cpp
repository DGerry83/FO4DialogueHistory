#include "Core/Message/MessageContentFilter.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
	const std::vector<std::string> kKnownPrefixes{
		"|HUDFramework|",
		"| CompanionStatusEnhanced |"
	};
}

TEST_CASE("message content rejects empty text", "[message]")
{
	CHECK_FALSE(F4DH::Core::MessageContentFilter::ShouldCapture("", kKnownPrefixes));
	CHECK_FALSE(F4DH::Core::MessageContentFilter::ShouldCapture("", {}));
}

TEST_CASE("message content rejects configured protocol packets", "[message]")
{
	CHECK_FALSE(F4DH::Core::MessageContentFilter::ShouldCapture("|HUDFramework|GenericUpdate|10|100|854.080200|0.000000|0.000", kKnownPrefixes));
	CHECK_FALSE(F4DH::Core::MessageContentFilter::ShouldCapture("|HUDFramework|", kKnownPrefixes));
	CHECK_FALSE(F4DH::Core::MessageContentFilter::ShouldCapture("| CompanionStatusEnhanced | UpdateStats | 3 | 1.000000 | AP ", kKnownPrefixes));
	CHECK_FALSE(F4DH::Core::MessageContentFilter::ShouldCapture("| CompanionStatusEnhanced | SetFollowerNameAndIcon | 3 | Heather", kKnownPrefixes));
}

TEST_CASE("message content keeps ordinary and numeric messages", "[message]")
{
	CHECK(F4DH::Core::MessageContentFilter::ShouldCapture("42", kKnownPrefixes));
	CHECK(F4DH::Core::MessageContentFilter::ShouldCapture("You are Severely Dehydrated.", kKnownPrefixes));
	CHECK(F4DH::Core::MessageContentFilter::ShouldCapture("CDDER - Mod Loading", kKnownPrefixes));
	CHECK(F4DH::Core::MessageContentFilter::ShouldCapture(" |HUDFramework|GenericUpdate|not-a-protocol-prefix", kKnownPrefixes));
}

TEST_CASE("message exclusions require exact case-sensitive prefixes", "[message]")
{
	CHECK(F4DH::Core::MessageContentFilter::ShouldCapture("|hudframework|GenericUpdate", kKnownPrefixes));
	CHECK(F4DH::Core::MessageContentFilter::ShouldCapture("prefix |HUDFramework| appears later", kKnownPrefixes));
	CHECK(F4DH::Core::MessageContentFilter::ShouldCapture("| CompanionStatusEnhanced| missing space", kKnownPrefixes));
	CHECK(F4DH::Core::MessageContentFilter::ShouldCapture("anything", {}));
}

TEST_CASE("message exclusion matcher reports the matching rule", "[message]")
{
	CHECK(F4DH::Core::MessageContentFilter::FindExcludedPrefix("|HUDFramework|GenericUpdate", kKnownPrefixes) == "|HUDFramework|");
	CHECK(F4DH::Core::MessageContentFilter::FindExcludedPrefix("| CompanionStatusEnhanced | UpdateStats", kKnownPrefixes) == "| CompanionStatusEnhanced |");
	CHECK(F4DH::Core::MessageContentFilter::FindExcludedPrefix("ordinary", kKnownPrefixes).empty());
}

TEST_CASE("message exclusion parser reads repeated section entries", "[message]")
{
	const std::vector<std::string> lines{
		"; full-line comment",
		"# another comment",
		"[Settings]",
		"ExcludedPrefix = ignored-in-other-section",
		"[MessageFilters]",
		"ExcludedPrefix = |HUDFramework|",
		"  ExcludedPrefix  =   | CompanionStatusEnhanced |  ",
		"OtherKey = ignored",
		"ExcludedPrefix =    ",
		"not a key/value line",
		"ExcludedPrefix = |CaseSensitive|",
	};
	const auto prefixes = F4DH::Core::MessageContentFilter::ParseExcludedPrefixes(lines);
	REQUIRE(prefixes.size() == 3);
	CHECK(prefixes[0] == "|HUDFramework|");
	CHECK(prefixes[1] == "| CompanionStatusEnhanced |");
	CHECK(prefixes[2] == "|CaseSensitive|");
}

TEST_CASE("message exclusion parser leaves and re-enters the target section", "[message]")
{
	const std::vector<std::string> lines{
		"[MessageFilters]",
		"ExcludedPrefix = |First|",
		"[Other]",
		"ExcludedPrefix = |Ignored|",
		"[messagefilters]",
		"excludedprefix = |Second|",
		"# ExcludedPrefix = |Commented|",
	};
	const auto prefixes = F4DH::Core::MessageContentFilter::ParseExcludedPrefixes(lines);
	REQUIRE(prefixes.size() == 2);
	CHECK(prefixes[0] == "|First|");
	CHECK(prefixes[1] == "|Second|");
}

TEST_CASE("message exclusion parser returns an empty set without valid entries", "[message]")
{
	CHECK(F4DH::Core::MessageContentFilter::ParseExcludedPrefixes({}).empty());
	CHECK(F4DH::Core::MessageContentFilter::ParseExcludedPrefixes({ "[MessageFilters]", "ExcludedPrefix =" }).empty());
	CHECK(F4DH::Core::MessageContentFilter::ParseExcludedPrefixes({ "[Settings]", "ExcludedPrefix = |Ignored|" }).empty());
}
