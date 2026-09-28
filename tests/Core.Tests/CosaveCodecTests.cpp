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
		const std::string& questName = "",
		std::uint8_t questType = 0)
	{
		F4DH::Core::DialogueLine line;
		line.speaker = speaker;
		line.kind = kind;
		line.text = text;
		line.questId = questId;
		line.questName = questName;
		line.questType = questType;
		return line;
	}

	std::vector<F4DH::Core::DialogueLine> RoundTrip(const std::vector<F4DH::Core::DialogueLine>& lines)
	{
		const auto bytes = F4DH::Core::CosaveCodec::Encode(lines);
		std::vector<F4DH::Core::DialogueLine> out;
		REQUIRE(F4DH::Core::CosaveCodec::Decode(bytes, out, 2));
		return out;
	}

	void RequireEqual(const F4DH::Core::DialogueLine& a, const F4DH::Core::DialogueLine& b)
	{
		REQUIRE(a.speaker == b.speaker);
		REQUIRE(a.kind == b.kind);
		REQUIRE(a.text == b.text);
		REQUIRE(a.questId == b.questId);
		REQUIRE(a.questName == b.questName);
		REQUIRE(a.questType == b.questType);
	}
}

TEST_CASE("codec round-trips lines with quest fields")
{
	const std::vector<F4DH::Core::DialogueLine> lines{
		MakeLine("Piper", F4DH::Core::SpeakerKind::Npc, "You okay?", 1234567, "The Molecular Level", 1),
		MakeLine("Player", F4DH::Core::SpeakerKind::Player, "Fine."),
		MakeLine("Nick \"Valentine\"", F4DH::Core::SpeakerKind::Npc, "Line\nwith\tcontrols", 0xFFFFFFFF, "a\\b", 8),
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
	REQUIRE(F4DH::Core::CosaveCodec::Decode(bytes, out, 2));
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
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(bytes, out, 2));
	REQUIRE(out.empty());
}

TEST_CASE("decode rejects a payload truncated inside a line")
{
	const auto bytes = F4DH::Core::CosaveCodec::Encode(
		{ MakeLine("Piper", F4DH::Core::SpeakerKind::Npc, "You okay?", 42, "Quest") });
	REQUIRE(bytes.size() > 3);

	std::vector<F4DH::Core::DialogueLine> out;
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(
		std::span<const std::byte>(bytes.data(), bytes.size() - 3), out, 2));
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
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(bytes, out, 1));
	REQUIRE(out.empty());
}

TEST_CASE("decode rejects trailing bytes after a zero-count payload")
{
	const std::vector<std::byte> validEmpty{ std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 } };
	const std::vector<std::byte> withJunk{ std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0xAA } };
	std::vector<F4DH::Core::DialogueLine> out;
	for (const std::uint32_t version : { 1u, 2u }) {
		REQUIRE(F4DH::Core::CosaveCodec::Decode(validEmpty, out, version));
		REQUIRE(out.empty());
		REQUIRE(!F4DH::Core::CosaveCodec::Decode(withJunk, out, version));
	}
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
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(bytes, out, 1));
	REQUIRE(out.empty());
}


namespace
{
	void PushU32(std::vector<std::byte>& a_bytes, std::uint32_t a_value)
	{
		for (unsigned shift = 0; shift < 32; shift += 8) {
			a_bytes.push_back(static_cast<std::byte>((a_value >> shift) & 0xFF));
		}
	}

	void PushString(std::vector<std::byte>& a_bytes, const std::string& a_value)
	{
		PushU32(a_bytes, static_cast<std::uint32_t>(a_value.size()));
		a_bytes.insert(a_bytes.end(), reinterpret_cast<const std::byte*>(a_value.data()),
			reinterpret_cast<const std::byte*>(a_value.data() + a_value.size()));
	}

	// Hand-built v1 layout: kind | questId | speaker | text | questName —
	// no questType byte (mirrors pre-v2 saves).
	std::vector<std::byte> BuildV1Payload(const std::vector<F4DH::Core::DialogueLine>& a_lines)
	{
		std::vector<std::byte> bytes;
		PushU32(bytes, static_cast<std::uint32_t>(a_lines.size()));
		for (const auto& line : a_lines) {
			bytes.push_back(static_cast<std::byte>(line.kind));
			PushU32(bytes, line.questId);
			PushString(bytes, line.speaker);
			PushString(bytes, line.text);
			PushString(bytes, line.questName);
		}
		return bytes;
	}
}

TEST_CASE("v2 round-trip preserves questType")
{
	const std::vector<F4DH::Core::DialogueLine> lines{
		MakeLine("Piper", F4DH::Core::SpeakerKind::Npc, "You okay?", 1234567, "The Molecular Level", 1),
		MakeLine("Player", F4DH::Core::SpeakerKind::Player, "Fine."),
		MakeLine("Nick", F4DH::Core::SpeakerKind::Npc, "DLC line", 42, "Far Harbor", 10),
	};

	const auto out = RoundTrip(lines);
	REQUIRE(out.size() == lines.size());
	for (std::size_t i = 0; i < lines.size(); ++i) {
		RequireEqual(out[i], lines[i]);
	}
}

TEST_CASE("hand-constructed v1 payload decodes with questType 0 and all other fields intact")
{
	const std::vector<F4DH::Core::DialogueLine> lines{
		// questType 9 here only proves the v1 builder drops it — a v1 save
		// carries no type byte, so the decoded line must come back 0.
		MakeLine("Piper", F4DH::Core::SpeakerKind::Npc, "You okay?", 1234567, "The Molecular Level", 9),
		MakeLine("Player", F4DH::Core::SpeakerKind::Player, "Fine."),
	};
	const auto v1 = BuildV1Payload(lines);

	std::vector<F4DH::Core::DialogueLine> out;
	REQUIRE(F4DH::Core::CosaveCodec::Decode(v1, out, 1));
	REQUIRE(out.size() == lines.size());
	for (std::size_t i = 0; i < lines.size(); ++i) {
		REQUIRE(out[i].speaker == lines[i].speaker);
		REQUIRE(out[i].kind == lines[i].kind);
		REQUIRE(out[i].text == lines[i].text);
		REQUIRE(out[i].questId == lines[i].questId);
		REQUIRE(out[i].questName == lines[i].questName);
		REQUIRE(out[i].questType == 0);
	}
}

TEST_CASE("v1 payload fails closed when decoded as v2")
{
	const auto v1 = BuildV1Payload(
		{ MakeLine("Piper", F4DH::Core::SpeakerKind::Npc, "You okay?", 42, "Quest") });

	std::vector<F4DH::Core::DialogueLine> out;
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(v1, out, 2));  // missing questType byte
}

TEST_CASE("v2 payload with trailing garbage fails closed")
{
	const auto bytes = F4DH::Core::CosaveCodec::Encode(
		{ MakeLine("Piper", F4DH::Core::SpeakerKind::Npc, "You okay?", 42, "Quest", 6) });
	std::vector<std::byte> withJunk(bytes);
	withJunk.push_back(std::byte{ 0xAA });

	std::vector<F4DH::Core::DialogueLine> out;
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(withJunk, out, 2));
}

TEST_CASE("v1 payload with trailing garbage fails closed")
{
	auto v1 = BuildV1Payload(
		{ MakeLine("Piper", F4DH::Core::SpeakerKind::Npc, "You okay?", 42, "Quest") });
	v1.push_back(std::byte{ 0xAA });

	std::vector<F4DH::Core::DialogueLine> out;
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(v1, out, 1));
}

TEST_CASE("decode fails closed on an unknown record version")
{
	const auto bytes = F4DH::Core::CosaveCodec::Encode(
		{ MakeLine("Piper", F4DH::Core::SpeakerKind::Npc, "You okay?", 42, "Quest", 6) });
	std::vector<F4DH::Core::DialogueLine> out;
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(bytes, out, 3));
	REQUIRE(!F4DH::Core::CosaveCodec::Decode(bytes, out, 0));
	REQUIRE(out.empty());
}
