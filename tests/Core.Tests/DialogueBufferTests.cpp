#include <catch2/catch_test_macros.hpp>

#include "Core/Dialogue/DialogueBuffer.h"
#include "Core/Dialogue/DialogueLine.h"

namespace
{
	F4DH::Core::DialogueLine MakeLine(const std::string& speaker, const std::string& text, std::uint32_t questId = 0)
	{
		F4DH::Core::DialogueLine line;
		line.speaker = speaker;
		line.kind = F4DH::Core::SpeakerKind::Npc;
		line.text = text;
		line.questId = questId;
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

TEST_CASE("zero capacity is unlimited - no eviction")
{
	F4DH::Core::DialogueBuffer buffer(0);
	for (int i = 0; i < 600; ++i) {
		buffer.Push(MakeLine("a", std::to_string(i)));
	}

	REQUIRE(buffer.Capacity() == 0);
	REQUIRE(buffer.Size() == 600);
	const auto snapshot = buffer.Snapshot();
	REQUIRE(snapshot.size() == 600);
	REQUIRE(snapshot.front().text == "0");  // oldest first, nothing evicted
	REQUIRE(snapshot.back().text == "599");
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

TEST_CASE("removeQuest removes only the lines with the matching questId")
{
	F4DH::Core::DialogueBuffer buffer(10);
	buffer.Push(MakeLine("a", "1", 100));
	buffer.Push(MakeLine("b", "2", 200));
	buffer.Push(MakeLine("c", "3", 100));
	buffer.Push(MakeLine("d", "4", 0));

	REQUIRE(buffer.RemoveQuest(100) == 2);
	REQUIRE(buffer.Size() == 2);
	const auto lines = buffer.Snapshot();
	REQUIRE(lines[0].speaker == "b");
	REQUIRE(lines[0].questId == 200);
	REQUIRE(lines[1].speaker == "d");
	REQUIRE(lines[1].questId == 0);
}

TEST_CASE("removeQuest handles the unattributed questId 0 bucket")
{
	F4DH::Core::DialogueBuffer buffer(10);
	buffer.Push(MakeLine("a", "1", 0));
	buffer.Push(MakeLine("b", "2", 42));
	buffer.Push(MakeLine("c", "3", 0));

	REQUIRE(buffer.RemoveQuest(0) == 2);
	REQUIRE(buffer.Size() == 1);
	const auto lines = buffer.Snapshot();
	REQUIRE(lines[0].speaker == "b");
	REQUIRE(lines[0].questId == 42);
}

TEST_CASE("removeQuest returns the removed count")
{
	F4DH::Core::DialogueBuffer buffer(10);
	buffer.Push(MakeLine("a", "1", 7));
	buffer.Push(MakeLine("b", "2", 7));
	buffer.Push(MakeLine("c", "3", 7));

	REQUIRE(buffer.RemoveQuest(7) == 3);
	REQUIRE(buffer.RemoveQuest(7) == 0);
	REQUIRE(buffer.Size() == 0);
}

TEST_CASE("removeQuest is a no-op when the questId is absent")
{
	F4DH::Core::DialogueBuffer buffer(10);
	buffer.Push(MakeLine("a", "1", 100));
	buffer.Push(MakeLine("b", "2", 200));

	REQUIRE(buffer.RemoveQuest(999) == 0);
	REQUIRE(buffer.Size() == 2);
}

TEST_CASE("removeQuest on an empty buffer returns zero")
{
	F4DH::Core::DialogueBuffer buffer(10);

	REQUIRE(buffer.RemoveQuest(100) == 0);
	REQUIRE(buffer.Size() == 0);
}

TEST_CASE("removeQuest preserves the order of the remaining lines")
{
	F4DH::Core::DialogueBuffer buffer(10);
	buffer.Push(MakeLine("a", "1", 1));
	buffer.Push(MakeLine("b", "2", 2));
	buffer.Push(MakeLine("c", "3", 1));
	buffer.Push(MakeLine("d", "4", 3));
	buffer.Push(MakeLine("e", "5", 2));
	buffer.Push(MakeLine("f", "6", 1));

	REQUIRE(buffer.RemoveQuest(1) == 3);
	const auto lines = buffer.Snapshot();
	REQUIRE(lines.size() == 3);
	REQUIRE(lines[0].text == "2");
	REQUIRE(lines[1].text == "4");
	REQUIRE(lines[2].text == "5");
}
