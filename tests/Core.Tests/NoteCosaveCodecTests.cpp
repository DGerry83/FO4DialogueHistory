#include <catch2/catch_test_macros.hpp>

#include "Core/Note/NoteCosaveCodec.h"
#include "Core/Note/NoteStore.h"

using namespace F4DH::Core;

namespace
{
	NoteRecord Record(NoteState state)
	{
		NoteRecord record;
		record.identity = { 7, NoteIdentityKind::DynamicSynthetic, 0x1234, 0xFF000001, 0x34, 2, 0, "Test.esp" };
		record.title = "Note: body";
		record.firstReadSequence = state == NoteState::Read ? 4 : 0;
		record.state = state;
		record.bodyMode = NoteBodyMode::CachedResolved;
		record.cachedBody = "resolved body\nwith text";
		record.firstEncounter = EncounterMetadata{ EncounterKind::Pickup, { 2287, 10, 29, 9, 30 }, "Diamond City" };
		return record;
	}

	void PushU32(std::vector<std::byte>& bytes, std::uint32_t value)
	{
		for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::byte>((value >> shift) & 0xFF));
	}
}

TEST_CASE("note codec round-trips pending and read records")
{
	const auto bytes = NoteCosaveCodec::Encode({ Record(NoteState::PendingPickup), Record(NoteState::Read) });
	std::vector<NoteRecord> out;
	REQUIRE(NoteCosaveCodec::Decode(bytes, out, 1));
	REQUIRE(out.size() == 2);
	REQUIRE(out[0].state == NoteState::PendingPickup);
	REQUIRE(out[0].firstReadSequence == 0);
	REQUIRE(out[1].identity.entryId == 7);
	REQUIRE(out[1].identity.sourceRefFormID == 0xFF000001);
	REQUIRE(out[1].identity.instanceOrdinal == 2);
	REQUIRE(out[1].title == "Note: body");
	REQUIRE(out[1].cachedBody == "resolved body\nwith text");
	REQUIRE(out[1].firstEncounter->kind == EncounterKind::Pickup);
	REQUIRE(out[1].firstEncounter->location == "Diamond City");
}

TEST_CASE("note codec empty payload is a zero count")
{
	const auto bytes = NoteCosaveCodec::Encode({});
	REQUIRE(bytes.size() == 4);
	std::vector<NoteRecord> out;
	REQUIRE(NoteCosaveCodec::Decode(bytes, out, 1));
	REQUIRE(out.empty());
}

TEST_CASE("note codec rejects unknown versions truncation and trailing data")
{
	const auto bytes = NoteCosaveCodec::Encode({ Record(NoteState::Read) });
	std::vector<NoteRecord> out;
	REQUIRE(!NoteCosaveCodec::Decode(bytes, out, 0));
	REQUIRE(!NoteCosaveCodec::Decode(bytes, out, 2));
	REQUIRE(!NoteCosaveCodec::Decode(std::span<const std::byte>(bytes.data(), bytes.size() - 1), out, 1));
	auto trailing = bytes;
	trailing.push_back(std::byte{ 0xAA });
	REQUIRE(!NoteCosaveCodec::Decode(trailing, out, 1));
	REQUIRE(out.empty());
}

TEST_CASE("note codec rejects excessive counts and string lengths without allocating")
{
	std::vector<std::byte> hugeCount;
	PushU32(hugeCount, NoteCosaveCodec::kMaxNotes + 1);
	std::vector<NoteRecord> out;
	REQUIRE(!NoteCosaveCodec::Decode(hugeCount, out, 1));

	std::vector<std::byte> hugeString;
	PushU32(hugeString, 1);
	PushU32(hugeString, 0);  // entryId low
	PushU32(hugeString, 0);  // entryId high
	PushU32(hugeString, 0);  // sequence low
	PushU32(hugeString, 0);  // sequence high
	hugeString.insert(hugeString.end(), 25, std::byte{ 0 });
	PushU32(hugeString, NoteCosaveCodec::kMaxStringBytes + 1);
	REQUIRE(!NoteCosaveCodec::Decode(hugeString, out, 1));
}

TEST_CASE("note codec fails closed on invalid enum values")
{
	auto bytes = NoteCosaveCodec::Encode({ Record(NoteState::Read) });
	bytes[20] = std::byte{ 0xFF };
	std::vector<NoteRecord> out;
	REQUIRE(!NoteCosaveCodec::Decode(bytes, out, 1));
	REQUIRE(out.empty());
}

TEST_CASE("note decoded records restore store sequence counters")
{
	NoteStore store;
	store.ReplaceAll({ Record(NoteState::Read) });
	NoteIdentity next;
	next.baseFormID = 0x9999;
	const auto change = store.ObserveRead(next, "Next", "body", NoteBodyMode::ResolveStatic,
		EncounterMetadata{ EncounterKind::Read, { 2287, 10, 29, 10, 0 }, "Cell" });
	REQUIRE(change.entryId == 8);
	REQUIRE(store.Find(change.entryId)->firstReadSequence == 5);
}

TEST_CASE("note codec encode rejects oversized collections and strings")
{
	std::vector<NoteRecord> tooMany(NoteCosaveCodec::kMaxNotes + 1);
	REQUIRE(NoteCosaveCodec::Encode(tooMany).empty());
	auto record = Record(NoteState::Read);
	record.cachedBody.assign(NoteCosaveCodec::kMaxStringBytes + 1, 'x');
	REQUIRE(NoteCosaveCodec::Encode({ record }).empty());
}
