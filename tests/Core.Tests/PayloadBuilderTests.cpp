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
		"[{\"speaker\":\"Piper\",\"kind\":\"npc\",\"text\":\"You okay?\",\"questId\":0,\"questName\":\"\",\"questType\":0},"
		"{\"speaker\":\"Player\",\"kind\":\"player\",\"text\":\"Fine.\",\"questId\":0,\"questName\":\"\",\"questType\":0}]");
}

TEST_CASE("append produces a single JSON object")
{
	const auto line = MakeLine("Player", F4DH::Core::SpeakerKind::Player, "Let's move.");
	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"Player\",\"kind\":\"player\",\"text\":\"Let's move.\",\"questId\":0,\"questName\":\"\",\"questType\":0}");
}

TEST_CASE("kind maps to player, npc, or unknown")
{
	const std::vector<F4DH::Core::DialogueLine> lines{
		MakeLine("a", F4DH::Core::SpeakerKind::Player, ""),
		MakeLine("b", F4DH::Core::SpeakerKind::Npc, ""),
		MakeLine("c", F4DH::Core::SpeakerKind::Unknown, ""),
	};

	REQUIRE(F4DH::Core::PayloadBuilder::BuildSnapshot(lines) ==
		"[{\"speaker\":\"a\",\"kind\":\"player\",\"text\":\"\",\"questId\":0,\"questName\":\"\",\"questType\":0},"
		"{\"speaker\":\"b\",\"kind\":\"npc\",\"text\":\"\",\"questId\":0,\"questName\":\"\",\"questType\":0},"
		"{\"speaker\":\"c\",\"kind\":\"unknown\",\"text\":\"\",\"questId\":0,\"questName\":\"\",\"questType\":0}]");
}

TEST_CASE("quotes and backslashes are escaped")
{
	const auto line = MakeLine("s", F4DH::Core::SpeakerKind::Npc, "say \"hi\" \\o/");
	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"s\",\"kind\":\"npc\",\"text\":\"say \\\"hi\\\" \\\\o/\",\"questId\":0,\"questName\":\"\",\"questType\":0}");
}

TEST_CASE("newline, carriage return, and tab are escaped")
{
	const auto line = MakeLine("s", F4DH::Core::SpeakerKind::Npc, "a\nb\rc\td");
	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"s\",\"kind\":\"npc\",\"text\":\"a\\nb\\rc\\td\",\"questId\":0,\"questName\":\"\",\"questType\":0}");
}

TEST_CASE("other control characters are escaped as lowercase unicode")
{
	const auto line = MakeLine("s", F4DH::Core::SpeakerKind::Npc, std::string("x\x01\x1fy"));
	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"s\",\"kind\":\"npc\",\"text\":\"x\\u0001\\u001fy\",\"questId\":0,\"questName\":\"\",\"questType\":0}");
}

TEST_CASE("speaker field is escaped")
{
	const auto line = MakeLine("Nick \"Valentine\"", F4DH::Core::SpeakerKind::Npc, "text");
	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"Nick \\\"Valentine\\\"\",\"kind\":\"npc\",\"text\":\"text\",\"questId\":0,\"questName\":\"\",\"questType\":0}");
}

TEST_CASE("payload carries questId and questName in snapshot and append")
{
	F4DH::Core::DialogueLine line;
	line.speaker = "Piper";
	line.kind = F4DH::Core::SpeakerKind::Npc;
	line.text = "You okay?";
	line.questId = 1234567;
	line.questName = "The Molecular Level";

	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"Piper\",\"kind\":\"npc\",\"text\":\"You okay?\","
		"\"questId\":1234567,\"questName\":\"The Molecular Level\",\"questType\":0}");

	REQUIRE(F4DH::Core::PayloadBuilder::BuildSnapshot({ line }) ==
		"[{\"speaker\":\"Piper\",\"kind\":\"npc\",\"text\":\"You okay?\","
		"\"questId\":1234567,\"questName\":\"The Molecular Level\",\"questType\":0}]");
}

TEST_CASE("unattributed line serializes as questId 0 and empty questName")
{
	const auto line = MakeLine("Player", F4DH::Core::SpeakerKind::Player, "Fine.");
	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"Player\",\"kind\":\"player\",\"text\":\"Fine.\","
		"\"questId\":0,\"questName\":\"\",\"questType\":0}");
}

TEST_CASE("quest name is escaped in the payload")
{
	F4DH::Core::DialogueLine line;
	line.speaker = "s";
	line.kind = F4DH::Core::SpeakerKind::Npc;
	line.text = "t";
	line.questId = 7;
	line.questName = "say \"hi\"";
	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"s\",\"kind\":\"npc\",\"text\":\"t\","
		"\"questId\":7,\"questName\":\"say \\\"hi\\\"\",\"questType\":0}");
}

TEST_CASE("payload carries the raw quest type numerically")
{
	F4DH::Core::DialogueLine line;
	line.speaker = "Piper";
	line.kind = F4DH::Core::SpeakerKind::Npc;
	line.text = "You okay?";
	line.questId = 1234567;
	line.questName = "The Molecular Level";
	line.questType = 7;  // kSideQuest

	REQUIRE(F4DH::Core::PayloadBuilder::BuildAppend(line) ==
		"{\"speaker\":\"Piper\",\"kind\":\"npc\",\"text\":\"You okay?\","
		"\"questId\":1234567,\"questName\":\"The Molecular Level\",\"questType\":7}");

	REQUIRE(F4DH::Core::PayloadBuilder::BuildSnapshot({ line }) ==
		"[{\"speaker\":\"Piper\",\"kind\":\"npc\",\"text\":\"You okay?\","
		"\"questId\":1234567,\"questName\":\"The Molecular Level\",\"questType\":7}]");
}

