#include <catch2/catch_test_macros.hpp>

#include "Core/Dialogue/DialogueBuffer.h"
#include "Core/Dialogue/DialogueLine.h"

namespace
{
	F4DH::Core::DialogueLine MakeLine(const std::string& speaker, const std::string& text)
	{
		F4DH::Core::DialogueLine line;
		line.speaker = speaker;
		line.kind = F4DH::Core::SpeakerKind::Npc;
		line.text = text;
		return line;
	}
}

TEST_CASE("default capacity is 50")
{
	const F4DH::Core::DialogueBuffer buffer;
	REQUIRE(buffer.Capacity() == 50);
	REQUIRE(buffer.Size() == 0);
}

TEST_CASE("custom capacity is respected")
{
	F4DH::Core::DialogueBuffer buffer(3);
	buffer.Push(MakeLine("a", "1"));
	buffer.Push(MakeLine("b", "2"));
	buffer.Push(MakeLine("c", "3"));
	REQUIRE(buffer.Size() == 3);
	REQUIRE(buffer.Capacity() == 3);
}

TEST_CASE("pushing beyond capacity evicts the oldest line")
{
	F4DH::Core::DialogueBuffer buffer(2);
	buffer.Push(MakeLine("a", "1"));
	buffer.Push(MakeLine("b", "2"));
	buffer.Push(MakeLine("c", "3"));

	REQUIRE(buffer.Size() == 2);
	const auto lines = buffer.Snapshot();
	REQUIRE(lines.size() == 2);
	REQUIRE(lines[0].speaker == "b");
	REQUIRE(lines[1].speaker == "c");
}

TEST_CASE("snapshot returns lines oldest-first")
{
	F4DH::Core::DialogueBuffer buffer(10);
	buffer.Push(MakeLine("first", "1"));
	buffer.Push(MakeLine("second", "2"));
	buffer.Push(MakeLine("third", "3"));

	const auto lines = buffer.Snapshot();
	REQUIRE(lines.size() == 3);
	REQUIRE(lines[0].speaker == "first");
	REQUIRE(lines[1].speaker == "second");
	REQUIRE(lines[2].speaker == "third");
}

TEST_CASE("clear empties the buffer")
{
	F4DH::Core::DialogueBuffer buffer(10);
	buffer.Push(MakeLine("a", "1"));
	buffer.Push(MakeLine("b", "2"));
	buffer.Clear();

	REQUIRE(buffer.Size() == 0);
	REQUIRE(buffer.Snapshot().empty());
}

TEST_CASE("zero-capacity buffer ignores pushes")
{
	F4DH::Core::DialogueBuffer buffer(0);
	buffer.Push(MakeLine("a", "1"));

	REQUIRE(buffer.Capacity() == 0);
	REQUIRE(buffer.Size() == 0);
	REQUIRE(buffer.Snapshot().empty());
}

TEST_CASE("snapshot round-trips quest attribution fields")
{
	F4DH::Core::DialogueBuffer buffer(3);

	F4DH::Core::DialogueLine attributed;
	attributed.speaker = "Piper";
	attributed.kind = F4DH::Core::SpeakerKind::Npc;
	attributed.text = "You okay?";
	attributed.questId = 1234567;
	attributed.questName = "The Molecular Level";
	buffer.Push(std::move(attributed));

	buffer.Push(MakeLine("b", "2"));

	const auto lines = buffer.Snapshot();
	REQUIRE(lines.size() == 2);
	REQUIRE(lines[0].questId == 1234567);
	REQUIRE(lines[0].questName == "The Molecular Level");
	REQUIRE(lines[1].questId == 0);
	REQUIRE(lines[1].questName.empty());
}
