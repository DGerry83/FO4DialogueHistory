#include <catch2/catch_test_macros.hpp>

#include "Core/Dialogue/PayloadBuilder.h"

namespace
{
	F4DH::Core::DialogueLine MakeLine(
		const std::string& speaker,
		F4DH::Core::SpeakerKind kind,
		const std::string& text)
	{
		F4DH::Core::DialogueLine line;
		line.speaker = speaker;
		line.kind = kind;
		line.text = text;
		return line;
	}
}

TEST_CASE("empty snapshot is an empty JSON array")
{
	REQUIRE(F4DH::Core::PayloadBuilder::BuildSnapshot({}) == "[]");
}

TEST_CASE("snapshot is a JSON array oldest-first with speaker, kind, and text fields")
{
	const std::vector<F4DH::Core::DialogueLine> lines{
		MakeLine("Piper", F4DH::Core::SpeakerKind::Npc, "You okay?"),
		MakeLine("Player", F4DH::Core::SpeakerKind::Player, "Fine."),
	};

	REQUIRE(F4DH::Core::PayloadBuilder::BuildSnapshot(lines) ==
		"[{\"speaker\":\"Piper\",\"kind\":\"npc\",\"text\":\"You okay?\"},"
		"{\"speaker\":\"Player\",\"kind\":\"player\",\"text\":\"Fine.\"}]");
}

TEST_CASE("append produces a single JSON object")
{
	const auto line = MakeLine("Player", F4DH::Core::SpeakerKind::Player, "Let's move.");
	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"Player\",\"kind\":\"player\",\"text\":\"Let's move.\"}");
}

TEST_CASE("kind maps to player, npc, or unknown")
{
	const std::vector<F4DH::Core::DialogueLine> lines{
		MakeLine("a", F4DH::Core::SpeakerKind::Player, ""),
		MakeLine("b", F4DH::Core::SpeakerKind::Npc, ""),
		MakeLine("c", F4DH::Core::SpeakerKind::Unknown, ""),
	};

	REQUIRE(F4DH::Core::PayloadBuilder::BuildSnapshot(lines) ==
		"[{\"speaker\":\"a\",\"kind\":\"player\",\"text\":\"\"},"
		"{\"speaker\":\"b\",\"kind\":\"npc\",\"text\":\"\"},"
		"{\"speaker\":\"c\",\"kind\":\"unknown\",\"text\":\"\"}]");
}

TEST_CASE("quotes and backslashes are escaped")
{
	const auto line = MakeLine("s", F4DH::Core::SpeakerKind::Npc, "say \"hi\" \\o/");
	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"s\",\"kind\":\"npc\",\"text\":\"say \\\"hi\\\" \\\\o/\"}");
}

TEST_CASE("newline, carriage return, and tab are escaped")
{
	const auto line = MakeLine("s", F4DH::Core::SpeakerKind::Npc, "a\nb\rc\td");
	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"s\",\"kind\":\"npc\",\"text\":\"a\\nb\\rc\\td\"}");
}

TEST_CASE("other control characters are escaped as lowercase unicode")
{
	const auto line = MakeLine("s", F4DH::Core::SpeakerKind::Npc, std::string("x\x01\x1fy"));
	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"s\",\"kind\":\"npc\",\"text\":\"x\\u0001\\u001fy\"}");
}

TEST_CASE("speaker field is escaped")
{
	const auto line = MakeLine("Nick \"Valentine\"", F4DH::Core::SpeakerKind::Npc, "text");
	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"Nick \\\"Valentine\\\"\",\"kind\":\"npc\",\"text\":\"text\"}");
}
