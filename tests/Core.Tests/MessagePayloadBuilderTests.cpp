#include <catch2/catch_test_macros.hpp>

#include "Core/Message/MessageLine.h"
#include "Core/Message/MessagePayloadBuilder.h"

namespace
{
	F4DH::Core::MessageLine MakeMessage(const std::string& text)
	{
		F4DH::Core::MessageLine message;
		message.text = text;
		return message;
	}
}

TEST_CASE("message empty snapshot is an empty JSON array")
{
	REQUIRE(F4DH::Core::MessagePayloadBuilder::BuildSnapshot({}) == "[]");
}

TEST_CASE("message snapshot is a JSON array oldest-first with a single text key")
{
	const std::vector<F4DH::Core::MessageLine> messages{
		MakeMessage("Quest added"),
		MakeMessage("Item added"),
	};

	REQUIRE(F4DH::Core::MessagePayloadBuilder::BuildSnapshot(messages) ==
		"[{\"text\":\"Quest added\"},{\"text\":\"Item added\"}]");
}

TEST_CASE("message append produces a single text-only JSON object")
{
	REQUIRE(F4DH::Core::MessagePayloadBuilder::BuildAppend(MakeMessage("You cannot go that way.")) ==
		"{\"text\":\"You cannot go that way.\"}");
}

TEST_CASE("message quotes and backslashes are escaped")
{
	REQUIRE(F4DH::Core::MessagePayloadBuilder::BuildAppend(MakeMessage("say \"hi\" \\o/")) ==
		"{\"text\":\"say \\\"hi\\\" \\\\o/\"}");
}

TEST_CASE("message newline, carriage return, and tab are escaped")
{
	REQUIRE(F4DH::Core::MessagePayloadBuilder::BuildAppend(MakeMessage("a\nb\rc\td")) ==
		"{\"text\":\"a\\nb\\rc\\td\"}");
}

TEST_CASE("message other control characters are escaped as lowercase unicode")
{
	REQUIRE(F4DH::Core::MessagePayloadBuilder::BuildAppend(MakeMessage(std::string("x\x01\x1fy"))) ==
		"{\"text\":\"x\\u0001\\u001fy\"}");
}

TEST_CASE("message utf-8 text passes through verbatim")
{
	// 2-byte (e-acute), 3-byte (em dash), and 4-byte (party popper) sequences
	const std::string utf8 = "caf\xC3\xA9 \xE2\x80\x94 \xF0\x9F\x8E\x89";
	REQUIRE(F4DH::Core::MessagePayloadBuilder::BuildAppend(MakeMessage(utf8)) ==
		"{\"text\":\"" + utf8 + "\"}");
}
