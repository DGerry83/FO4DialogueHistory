#include <catch2/catch_test_macros.hpp>

#include "Core/Message/MessageBuffer.h"
#include "Core/Message/MessageLine.h"

namespace
{
	F4DH::Core::MessageLine MakeMessage(const std::string& text)
	{
		F4DH::Core::MessageLine message;
		message.text = text;
		return message;
	}
}

TEST_CASE("message default capacity is 200")
{
	const F4DH::Core::MessageBuffer buffer;
	REQUIRE(buffer.Capacity() == 200);
	REQUIRE(buffer.Size() == 0);
}

TEST_CASE("message construction clamps capacities below 1 to 1")
{
	F4DH::Core::MessageBuffer buffer(0);
	REQUIRE(buffer.Capacity() == 1);

	buffer.Push(MakeMessage("a"));
	buffer.Push(MakeMessage("b"));
	const auto messages = buffer.Snapshot();
	REQUIRE(messages.size() == 1);
	REQUIRE(messages[0].text == "b");  // oldest evicted at capacity
}

TEST_CASE("message construction clamps capacities above 1000 to 1000")
{
	const F4DH::Core::MessageBuffer buffer(5000);
	REQUIRE(buffer.Capacity() == 1000);
}

TEST_CASE("message custom capacity is respected")
{
	F4DH::Core::MessageBuffer buffer(3);
	buffer.Push(MakeMessage("1"));
	buffer.Push(MakeMessage("2"));
	buffer.Push(MakeMessage("3"));
	REQUIRE(buffer.Size() == 3);
	REQUIRE(buffer.Capacity() == 3);
}

TEST_CASE("message pushing beyond capacity evicts the oldest")
{
	F4DH::Core::MessageBuffer buffer(2);
	buffer.Push(MakeMessage("1"));
	buffer.Push(MakeMessage("2"));
	buffer.Push(MakeMessage("3"));

	REQUIRE(buffer.Size() == 2);
	const auto messages = buffer.Snapshot();
	REQUIRE(messages.size() == 2);
	REQUIRE(messages[0].text == "2");
	REQUIRE(messages[1].text == "3");
}

TEST_CASE("message capacity of one retains only the newest")
{
	F4DH::Core::MessageBuffer buffer(1);
	buffer.Push(MakeMessage("1"));
	buffer.Push(MakeMessage("2"));
	buffer.Push(MakeMessage("3"));

	const auto messages = buffer.Snapshot();
	REQUIRE(messages.size() == 1);
	REQUIRE(messages[0].text == "3");
}

TEST_CASE("message capacity boundary 1000 retains the newest 1000")
{
	F4DH::Core::MessageBuffer buffer(1000);
	for (int i = 0; i < 1100; ++i) {
		buffer.Push(MakeMessage(std::to_string(i)));
	}

	const auto messages = buffer.Snapshot();
	REQUIRE(messages.size() == 1000);
	REQUIRE(messages.front().text == "100");
	REQUIRE(messages.back().text == "1099");
}

TEST_CASE("message snapshot returns messages oldest-first")
{
	F4DH::Core::MessageBuffer buffer(10);
	buffer.Push(MakeMessage("first"));
	buffer.Push(MakeMessage("second"));
	buffer.Push(MakeMessage("third"));

	const auto messages = buffer.Snapshot();
	REQUIRE(messages.size() == 3);
	REQUIRE(messages[0].text == "first");
	REQUIRE(messages[1].text == "second");
	REQUIRE(messages[2].text == "third");
}

TEST_CASE("message clear empties the buffer")
{
	F4DH::Core::MessageBuffer buffer(10);
	buffer.Push(MakeMessage("1"));
	buffer.Push(MakeMessage("2"));
	buffer.Clear();

	REQUIRE(buffer.Size() == 0);
	REQUIRE(buffer.Snapshot().empty());
}

TEST_CASE("message duplicates are retained as separate entries")
{
	F4DH::Core::MessageBuffer buffer(10);
	buffer.Push(MakeMessage("same"));
	buffer.Push(MakeMessage("same"));
	buffer.Push(MakeMessage("same"));

	const auto messages = buffer.Snapshot();
	REQUIRE(messages.size() == 3);
	REQUIRE(messages[0].text == "same");
	REQUIRE(messages[1].text == "same");
	REQUIRE(messages[2].text == "same");
}

TEST_CASE("message buffer never exceeds its configured limit")
{
	F4DH::Core::MessageBuffer buffer(5);
	for (int i = 0; i < 50; ++i) {
		buffer.Push(MakeMessage(std::to_string(i)));
	}

	const auto messages = buffer.Snapshot();
	REQUIRE(messages.size() == 5);
	REQUIRE(messages.front().text == "45");
	REQUIRE(messages.back().text == "49");
}
