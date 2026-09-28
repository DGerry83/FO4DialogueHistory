#include <catch2/catch_test_macros.hpp>

#include "Core/Dialogue/CosaveCodec.h"
#include "Core/Dialogue/DialogueBuffer.h"

namespace
{
	F4DH::Core::DialogueLine MakeLine(
		const std::string& speaker,
		F4DH::Core::SpeakerKind kind,
		const std::string& text,
		std::uint32_t questId = 0,
		const std::string& questName = "")
	{
		F4DH::Core::DialogueLine line;
		line.speaker = speaker;
		line.kind = kind;
		line.text = text;
		line.questId = questId;
		line.questName = questName;
		return line;
	}

	std::vector<F4DH::Core::DialogueLine> RoundTrip(const std::vector<F4DH::Core::DialogueLine>& lines)
	{
		const auto bytes = F4DH::Core::CosaveCodec::Encode(lines);
		std::vector<F4DH::Core::DialogueLine> out;
		REQUIRE(F4DH::Core::CosaveCodec::Decode(bytes, out));
		return out;
	}

	void RequireEqual(const F4DH::Core::DialogueLine& a, const F4DH::Core::DialogueLine& b)
	{
		REQUIRE(a.speaker == b.speaker);
		REQUIRE(a.kind == b.kind);
		REQUIRE(a.text == b.text);
		REQUIRE(a.questId == b.questId);
		REQUIRE(a.questName == b.questName);
	}
}

TEST_CASE("codec round-trips lines with quest fields")
{
	const std::vector<F4DH::Core::DialogueLine> lines{
		MakeLine("Piper", F4DH::Core::SpeakerKind::Npc, "You okay?", 1234567, "The Molecular Level"),
		MakeLine("Player", F4DH::Core::SpeakerKind::Player, "Fine."),
		MakeLine("Nick \"Valentine\"", F4DH::Core::SpeakerKind::Npc, "Line\nwith\tcontrols", 0xFFFFFFFF, "a\\b"),
		MakeLine("Unknown", F4DH::Core::SpeakerKind::Unknown, ""),
	};

	const auto out = RoundTrip(lines);
	REQUIRE(out.size() == lines.size());
	for (std::size_t i = 0; i < lines.size(); ++i) {
		RequireEqual(out[i], lines[i]);
	}
}

TEST_CASE("codec encodes an empty buffer as a zero line count")
{
	const auto bytes = F4DH::Core::CosaveCodec::Encode({});
	REQUIRE(bytes.size() == 4);
	for (const auto b : bytes) {
		REQUIRE(b == std::byte{ 0 });
	}

	std::vector<F4DH::Core::DialogueLine> out;
	REQUIRE(F4DH::Core::CosaveCodec::Decode(bytes, out));
	REQUIRE(out.empty());
}

TEST_CASE("decoded lines rebuild into the buffer respecting the 50-line capacity")
{
	std::vector<F4DH::Core::DialogueLine> lines;
	for (std::uint32_t i = 0; i < 60; ++i) {
		lines.push_back(MakeLine("s" + std::to_string(i), F4DH::Core::SpeakerKind::Npc,
			"line " + std::to_string(i), i, "quest " + std::to_string(i)));
	}

	F4DH::Core::DialogueBuffer buffer(50);  // store-side ring: Push evicts the oldest
	for (auto& line : RoundTrip(lines)) {
		buffer.Push(std::move(line));
	}

	const auto snapshot = buffer.Snapshot();
	REQUIRE(snapshot.size() == 50);
	for (std::size_t i = 0; i < snapshot.size(); ++i) {
		RequireEqual(snapshot[i], lines[i + 10]);  // lines 0-9 evicted
	}
}

TEST_CASE("decode rejects a payload shorter than the line count field")
{
	const std::vector<std::byte> bytes{ std::byte{ 1 }, std::byte{ 0 } };
	std::vector<F4DH::Core::DialogueLine> out;
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(bytes, out));
	REQUIRE(out.empty());
}

TEST_CASE("decode rejects a payload truncated inside a line")
{
	const auto bytes = F4DH::Core::CosaveCodec::Encode(
		{ MakeLine("Piper", F4DH::Core::SpeakerKind::Npc, "You okay?", 42, "Quest") });
	REQUIRE(bytes.size() > 3);

	std::vector<F4DH::Core::DialogueLine> out;
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(
		std::span<const std::byte>(bytes.data(), bytes.size() - 3), out));
}

TEST_CASE("decode rejects garbage length fields")
{
	// count=1, kind=Npc(1), questId=7, then a speaker length of 0xFFFFFFFF
	// with nothing after it.
	std::vector<std::byte> bytes;
	const auto pushU32 = [&bytes](std::uint32_t v) {
		for (unsigned shift = 0; shift < 32; shift += 8) {
			bytes.push_back(static_cast<std::byte>((v >> shift) & 0xFF));
		}
	};
	pushU32(1);
	bytes.push_back(std::byte{ 1 });
	pushU32(7);
	pushU32(0xFFFFFFFF);

	std::vector<F4DH::Core::DialogueLine> out;
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(bytes, out));
	REQUIRE(out.empty());
}

TEST_CASE("decode rejects trailing bytes after a zero-count payload")
{
	const std::vector<std::byte> validEmpty{ std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 } };
	std::vector<F4DH::Core::DialogueLine> out;
	REQUIRE(F4DH::Core::CosaveCodec::Decode(validEmpty, out));
	REQUIRE(out.empty());

	const std::vector<std::byte> withJunk{ std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0xAA } };
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(withJunk, out));
}

TEST_CASE("decode rejects an unknown kind byte")
{
	// count=1, kind=9 (not a valid SpeakerKind) — every other field present.
	std::vector<std::byte> bytes;
	const auto pushU32 = [&bytes](std::uint32_t v) {
		for (unsigned shift = 0; shift < 32; shift += 8) {
			bytes.push_back(static_cast<std::byte>((v >> shift) & 0xFF));
		}
	};
	pushU32(1);
	bytes.push_back(std::byte{ 9 });
	pushU32(0);
	pushU32(0);
	pushU32(0);
	pushU32(0);

	std::vector<F4DH::Core::DialogueLine> out;
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(bytes, out));
	REQUIRE(out.empty());
}
