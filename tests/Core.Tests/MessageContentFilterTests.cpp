#include "Core/Message/MessageContentFilter.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("message content rejects empty text", "[message]")
{
	CHECK_FALSE(F4DH::Core::MessageContentFilter::ShouldCapture(""));
}

TEST_CASE("message content rejects HUDFramework protocol packets", "[message]")
{
	CHECK_FALSE(F4DH::Core::MessageContentFilter::ShouldCapture("|HUDFramework|GenericUpdate|10|100|854.080200|0.000000|0.000"));
	CHECK_FALSE(F4DH::Core::MessageContentFilter::ShouldCapture("|HUDFramework|"));
}

TEST_CASE("message content keeps ordinary and numeric messages", "[message]")
{
	CHECK(F4DH::Core::MessageContentFilter::ShouldCapture("42"));
	CHECK(F4DH::Core::MessageContentFilter::ShouldCapture("You are Severely Dehydrated."));
	CHECK(F4DH::Core::MessageContentFilter::ShouldCapture(" |HUDFramework|GenericUpdate|not-a-protocol-prefix"));
}
