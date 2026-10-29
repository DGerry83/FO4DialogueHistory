#include <catch2/catch_test_macros.hpp>

#include "Core/Note/NoteStore.h"

using namespace F4DH::Core;

namespace
{
	NoteIdentity Identity(std::uint32_t base = 0x1000, NoteIdentityKind kind = NoteIdentityKind::StaticBase)
	{
		NoteIdentity id;
		id.baseFormID = base;
		id.localBaseFormID = base;
		id.kind = kind;
		id.pluginName = "Test.esp";
		return id;
	}

	EncounterMetadata Encounter(EncounterKind kind, std::uint8_t hour = 9)
	{
		EncounterMetadata first;
		first.kind = kind;
		first.timestamp = { 2287, 10, 29, hour, 30 };
		first.location = "Diamond City";
		return first;
	}
}

TEST_CASE("note pickup remains pending and invisible until read")
{
	NoteStore store;
	const auto pickup = store.ObservePickup(Identity(), "Dead Drop Note", Encounter(EncounterKind::Pickup));
	REQUIRE(store.Size() == 1);
	REQUIRE(store.VisibleSnapshot().empty());
	REQUIRE(!pickup.visibleChanged);

	const auto read = store.ObserveRead(Identity(), "Dead Drop Note", "body", NoteBodyMode::ResolveStatic, Encounter(EncounterKind::Read, 10));
	REQUIRE(read.entryId == pickup.entryId);
	REQUIRE(read.visibleChanged);
	REQUIRE(store.VisibleSnapshot().size() == 1);
	REQUIRE(store.Find(read.entryId)->firstEncounter->kind == EncounterKind::Pickup);
}

TEST_CASE("note read then pickup preserves the earlier read encounter")
{
	NoteStore store;
	const auto read = store.ObserveRead(Identity(), "Note", "body", NoteBodyMode::ResolveStatic, Encounter(EncounterKind::Read));
	(void)store.ObservePickup(Identity(), "Note", Encounter(EncounterKind::Pickup, 11));
	REQUIRE(store.Size() == 1);
	REQUIRE(store.Find(read.entryId)->firstEncounter->kind == EncounterKind::Read);
	REQUIRE(store.Find(read.entryId)->firstEncounter->timestamp.hour == 9);
}

TEST_CASE("note repeated reads and reacquisition are idempotent")
{
	NoteStore store;
	const auto first = store.ObserveRead(Identity(), "Note", "body", NoteBodyMode::ResolveStatic, Encounter(EncounterKind::Read));
	const auto second = store.ObserveRead(Identity(), "Different title", "new body", NoteBodyMode::ResolveStatic, Encounter(EncounterKind::Read, 12));
	(void)store.ObservePickup(Identity(), "Note", Encounter(EncounterKind::Pickup, 13));
	REQUIRE(store.Size() == 1);
	REQUIRE(second.entryId == first.entryId);
	REQUIRE(!second.visibleChanged);
	REQUIRE(store.Find(first.entryId)->title == "Note: body");
	REQUIRE(store.Find(first.entryId)->firstReadSequence == 1);
}

TEST_CASE("note static copies deduplicate by base form")
{
	NoteStore store;
	(void)store.ObserveRead(Identity(0x1000), "Note", "body", NoteBodyMode::ResolveStatic, Encounter(EncounterKind::Read));
	(void)store.ObserveRead(Identity(0x1000), "Note", "body", NoteBodyMode::ResolveStatic, Encounter(EncounterKind::Read));
	(void)store.ObserveRead(Identity(0x2000), "Note", "body", NoteBodyMode::ResolveStatic, Encounter(EncounterKind::Read));
	REQUIRE(store.VisibleSnapshot().size() == 2);
}

TEST_CASE("note dynamic references remain separate by source reference")
{
	NoteStore store;
	auto first = Identity(0x1000, NoteIdentityKind::DynamicReference);
	auto second = first;
	first.sourceRefFormID = 0xFF000001;
	second.sourceRefFormID = 0xFF000002;
	(void)store.ObserveRead(first, "Note", "first body", NoteBodyMode::CachedResolved, Encounter(EncounterKind::Read));
	(void)store.ObserveRead(second, "Note", "second body", NoteBodyMode::CachedResolved, Encounter(EncounterKind::Read));
	REQUIRE(store.VisibleSnapshot().size() == 2);
	REQUIRE(store.VisibleSnapshot()[0].cachedBody == "first body");
	REQUIRE(store.VisibleSnapshot()[1].cachedBody == "second body");
}

TEST_CASE("note world read pickup and inventory read preserve one dynamic entry")
{
	NoteStore store;
	auto world = Identity(0x1000, NoteIdentityKind::DynamicReference);
	world.sourceRefFormID = 0xFF000001;
	const std::string title = "Super Mutants in Trinity Plaza!";
	const std::string body = "Super Mutants have been spotted in Trinity Plaza.";
	const auto first = store.ObserveRead(world, title, body, NoteBodyMode::CachedResolved, Encounter(EncounterKind::Read));
	const auto folder = store.CreateFolder(0, "Quest notes");
	(void)store.AssignNote(first.entryId, folder.id);

	auto pickup = world;
	pickup.runtimeUniqueID = 3;
	(void)store.ObservePickup(pickup, "Super Mutants in <Alias=Dungeon>!", Encounter(EncounterKind::Pickup, 11));
	const auto inventory = Identity(0x1000, NoteIdentityKind::DynamicSynthetic);
	const auto second = store.ObserveRead(inventory, "Super Mutants in <Alias=Dungeon>!", body, NoteBodyMode::CachedResolved, Encounter(EncounterKind::Read, 12));

	REQUIRE(second.entryId == first.entryId);
	REQUIRE(!second.visibleChanged);
	REQUIRE(store.Size() == 1);
	const auto record = store.Find(first.entryId);
	REQUIRE(record->title == title);
	REQUIRE(record->firstEncounter->kind == EncounterKind::Read);
	REQUIRE(record->firstEncounter->timestamp.hour == 9);
	REQUIRE(record->firstReadSequence == 1);
	REQUIRE(record->folderId == folder.id);
	REQUIRE(record->cachedBody == body);
}

TEST_CASE("note synthetic instances with different resolved bodies do not merge")
{
	NoteStore store;
	const auto id = Identity(0x1000, NoteIdentityKind::DynamicSynthetic);
	(void)store.ObserveRead(id, "Note", "first body", NoteBodyMode::CachedResolved, Encounter(EncounterKind::Read));
	(void)store.ObserveRead(id, "Note", "second body", NoteBodyMode::CachedResolved, Encounter(EncounterKind::Read));
	const auto visible = store.VisibleSnapshot();
	REQUIRE(visible.size() == 2);
	REQUIRE(visible[0].identity.instanceOrdinal == 1);
	REQUIRE(visible[1].identity.instanceOrdinal == 2);
}

TEST_CASE("note pre-owned post-install read uses the read as first encounter")
{
	NoteStore store;
	const auto read = store.ObserveRead(Identity(), "Dead Drop Note", "body", NoteBodyMode::ResolveStatic, Encounter(EncounterKind::Read));
	REQUIRE(store.Size() == 1);
	REQUIRE(store.Find(read.entryId)->firstEncounter->kind == EncounterKind::Read);
}

TEST_CASE("note clear removes pending and read records without reusing sequences")
{
	NoteStore store;
	(void)store.ObservePickup(Identity(0x2000), "Pending", Encounter(EncounterKind::Pickup));
	(void)store.ObserveRead(Identity(), "Note", "body", NoteBodyMode::ResolveStatic, Encounter(EncounterKind::Read));
	store.Clear();
	REQUIRE(store.Size() == 0);
	REQUIRE(store.VisibleSnapshot().empty());
	const auto read = store.ObserveRead(Identity(), "Note", "body", NoteBodyMode::ResolveStatic, Encounter(EncounterKind::Read));
	REQUIRE(read.entryId == 3);
	REQUIRE(store.Find(3)->firstReadSequence == 2);
}

TEST_CASE("note replacement restores records and advances local sequences")
{
	NoteStore store;
	const auto record = store.Snapshot();
	(void)record;
	const auto read = store.ObserveRead(Identity(), "Note", "body", NoteBodyMode::CachedResolved, Encounter(EncounterKind::Read));
	auto records = store.Snapshot();
	store.Clear();
	store.ReplaceAll(records);
	const auto next = store.ObserveRead(Identity(0x2000), "Other", "body", NoteBodyMode::ResolveStatic, Encounter(EncounterKind::Read));
	REQUIRE(next.entryId == read.entryId + 1);
	REQUIRE(store.Find(next.entryId)->firstReadSequence == 2);
}

TEST_CASE("note store honors configured generic titles")
{
	NoteStore store;
	store.SetGenericTitles({ "Memo" });
	const auto change = store.ObserveRead(Identity(), "Memo", "opening", NoteBodyMode::ResolveStatic, Encounter(EncounterKind::Read));
	REQUIRE(store.Find(change.entryId)->title == "Memo: opening");
}

TEST_CASE("note synthetic repeated read without durable identity is idempotent")
{
	NoteStore store;
	const auto id = Identity(0x1000, NoteIdentityKind::DynamicSynthetic);
	const auto first = store.ObserveRead(id, "Note", "same body", NoteBodyMode::CachedResolved, Encounter(EncounterKind::Read));
	const auto second = store.ObserveRead(id, "Note", "same body", NoteBodyMode::CachedResolved, Encounter(EncounterKind::Read, 10));
	REQUIRE(second.entryId == first.entryId);
	REQUIRE(!second.visibleChanged);
	REQUIRE(store.VisibleSnapshot().size() == 1);
}
