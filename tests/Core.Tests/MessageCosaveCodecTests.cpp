#include <catch2/catch_test_macros.hpp>

#include "Core/Message/MessageBuffer.h"
#include "Core/Message/MessageCosaveCodec.h"
#include "Core/Message/MessageLine.h"

namespace
{
	F4DH::Core::MessageLine MakeMessage(const std::string& text)
	{
		F4DH::Core::MessageLine message;
		message.text = text;
		return message;
	}

	std::vector<F4DH::Core::MessageLine> RoundTrip(const std::vector<F4DH::Core::MessageLine>& messages)
	{
		const auto bytes = F4DH::Core::MessageCosaveCodec::Encode(messages);
		std::vector<F4DH::Core::MessageLine> out;
		REQUIRE(F4DH::Core::MessageCosaveCodec::Decode(bytes, out, 1));
		return out;
	}

	void PushU32(std::vector<std::byte>& a_bytes, std::uint32_t a_value)
	{
		for (unsigned shift = 0; shift < 32; shift += 8) {
			a_bytes.push_back(static_cast<std::byte>((a_value >> shift) & 0xFF));
		}
	}
}

TEST_CASE("message codec round-trips messages with resolved text")
{
	const std::vector<F4DH::Core::MessageLine> messages{
		MakeMessage("Quest added: The Molecular Level"),
		MakeMessage("say \"hi\" \\o/"),
		MakeMessage("line\nwith\tcontrols"),
		MakeMessage(""),              // codec carries empty text verbatim
		MakeMessage("caf\xC3\xA9"),  // UTF-8 bytes verbatim
	};

	const auto out = RoundTrip(messages);
	REQUIRE(out.size() == messages.size());
	for (std::size_t i = 0; i < messages.size(); ++i) {
		REQUIRE(out[i].text == messages[i].text);
	}
}

TEST_CASE("message codec encodes an empty buffer as a zero message count")
{
	const auto bytes = F4DH::Core::MessageCosaveCodec::Encode({});
	REQUIRE(bytes.size() == 4);
	for (const auto b : bytes) {
		REQUIRE(b == std::byte{ 0 });
	}

	std::vector<F4DH::Core::MessageLine> out;
	REQUIRE(F4DH::Core::MessageCosaveCodec::Decode(bytes, out, 1));
	REQUIRE(out.empty());
}

TEST_CASE("message decoded messages rebuild into the buffer respecting the capacity")
{
	std::vector<F4DH::Core::MessageLine> messages;
	for (std::uint32_t i = 0; i < 60; ++i) {
		messages.push_back(MakeMessage("message " + std::to_string(i)));
	}

	F4DH::Core::MessageBuffer buffer(50);  // store-side ring: Push evicts the oldest
	for (auto& message : RoundTrip(messages)) {
		buffer.Push(std::move(message));
	}

	const auto snapshot = buffer.Snapshot();
	REQUIRE(snapshot.size() == 50);
	for (std::size_t i = 0; i < snapshot.size(); ++i) {
		REQUIRE(snapshot[i].text == messages[i + 10].text);  // messages 0-9 evicted
	}
}

TEST_CASE("message decode rejects a payload shorter than the count field")
{
	const std::vector<std::byte> bytes{ std::byte{ 1 }, std::byte{ 0 } };
	std::vector<F4DH::Core::MessageLine> out;
	REQUIRE(!F4DH::Core::MessageCosaveCodec::Decode(bytes, out, 1));
	REQUIRE(out.empty());
}

TEST_CASE("message decode rejects a payload truncated inside a message")
{
	const auto bytes = F4DH::Core::MessageCosaveCodec::Encode({ MakeMessage("hello") });
	REQUIRE(bytes.size() > 2);

	std::vector<F4DH::Core::MessageLine> out;
	REQUIRE(!F4DH::Core::MessageCosaveCodec::Decode(
		std::span<const std::byte>(bytes.data(), bytes.size() - 2), out, 1));
}

TEST_CASE("message decode rejects a length field that exceeds the remaining data")
{
	// count=1, then a text length of 0xFFFFFFFF with nothing after it.
	std::vector<std::byte> bytes;
	PushU32(bytes, 1);
	PushU32(bytes, 0xFFFFFFFF);

	std::vector<F4DH::Core::MessageLine> out;
	REQUIRE(!F4DH::Core::MessageCosaveCodec::Decode(bytes, out, 1));
	REQUIRE(out.empty());
}

TEST_CASE("message decode fails closed on a corrupt count without a pre-allocation bomb")
{
	// A huge count with no message data at all must fail on the first string
	// read rather than reserving the count.
	const std::vector<std::byte> bytes{ std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF } };

	std::vector<F4DH::Core::MessageLine> out;
	REQUIRE(!F4DH::Core::MessageCosaveCodec::Decode(bytes, out, 1));
	REQUIRE(out.empty());
}

TEST_CASE("message decode rejects trailing bytes after a zero-count payload")
{
	const std::vector<std::byte> validEmpty{ std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 } };
	const std::vector<std::byte> withJunk{ std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0xAA } };
	std::vector<F4DH::Core::MessageLine> out;
	REQUIRE(F4DH::Core::MessageCosaveCodec::Decode(validEmpty, out, 1));
	REQUIRE(out.empty());
	REQUIRE(!F4DH::Core::MessageCosaveCodec::Decode(withJunk, out, 1));
}

TEST_CASE("message decode rejects trailing bytes after a valid payload")
{
	const auto bytes = F4DH::Core::MessageCosaveCodec::Encode({ MakeMessage("hello") });
	std::vector<std::byte> withJunk(bytes);
	withJunk.push_back(std::byte{ 0xAA });

	std::vector<F4DH::Core::MessageLine> out;
	REQUIRE(!F4DH::Core::MessageCosaveCodec::Decode(withJunk, out, 1));
}

TEST_CASE("message decode fails closed on any record version other than 1")
{
	const auto bytes = F4DH::Core::MessageCosaveCodec::Encode({ MakeMessage("hello") });
	std::vector<F4DH::Core::MessageLine> out;
	REQUIRE(!F4DH::Core::MessageCosaveCodec::Decode(bytes, out, 0));
	REQUIRE(!F4DH::Core::MessageCosaveCodec::Decode(bytes, out, 2));
	REQUIRE(out.empty());
}
