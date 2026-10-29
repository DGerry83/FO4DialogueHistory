#include <catch2/catch_test_macros.hpp>

#include "Core/Note/NoteCosaveCodec.h"
#include "Core/Note/NotePayloadBuilder.h"
#include "Core/Note/NoteStore.h"

using namespace F4DH::Core;

namespace
{
	EncounterMetadata Encounter()
	{
		return { EncounterKind::Read, { 2288, 1, 26, 9, 30 }, "Diamond City" };
	}

	NoteIdentity Identity(std::uint32_t base = 0x1000)
	{
		NoteIdentity id;
		id.baseFormID = base;
		return id;
	}

	NoteRecord ReadRecord(std::uint64_t entryId, std::uint64_t folderId)
	{
		NoteRecord record;
		record.identity.entryId = entryId;
		record.identity.baseFormID = 0x1000;
		record.title = "Note";
		record.firstReadSequence = entryId;
		record.state = NoteState::Read;
		record.bodyMode = NoteBodyMode::CachedResolved;
		record.cachedBody = "body";
		record.firstEncounter = Encounter();
		record.folderId = folderId;
		return record;
	}
}

TEST_CASE("folder creation nests to three levels and rejects level four")
{
	NoteStore store;
	const auto root = store.CreateFolder(0, "Root");
	const auto child = store.CreateFolder(root.id, "Child");
	const auto grandchild = store.CreateFolder(child.id, "Grandchild");
	REQUIRE(root.ok);
	REQUIRE(child.ok);
	REQUIRE(grandchild.ok);
	REQUIRE(store.Folders().size() == 3);
	REQUIRE(!store.CreateFolder(grandchild.id, "Too deep").ok);
}

TEST_CASE("folder creation rejects invalid parents duplicates and empty names")
{
	NoteStore store;
	REQUIRE(!store.CreateFolder(99, "Missing").ok);
	REQUIRE(!store.CreateFolder(0, "  ").ok);
	const auto root = store.CreateFolder(0, "Root");
	REQUIRE(!store.CreateFolder(0, "root").ok);
	(void)store.CreateFolder(root.id, "Child");
	REQUIRE(!store.CreateFolder(root.id, "child").ok);
}

TEST_CASE("folder creation enforces the 256 folder cap")
{
	NoteStore store;
	for (std::uint32_t i = 0; i < 256; ++i) {
		REQUIRE(store.CreateFolder(0, "Folder " + std::to_string(i)).ok);
	}
	REQUIRE(!store.CreateFolder(0, "Overflow").ok);
}

TEST_CASE("note assignment deletion and clear preserve folders and sequence counters")
{
	NoteStore store;
	const auto folder = store.CreateFolder(0, "Folder");
	const auto read = store.ObserveRead(Identity(), "Note", "body", NoteBodyMode::CachedResolved, Encounter());
	REQUIRE(store.AssignNote(read.entryId, folder.id).ok);
	REQUIRE(store.Find(read.entryId)->folderId == folder.id);
	REQUIRE(store.AssignNote(read.entryId, 0).ok);
	REQUIRE(store.Find(read.entryId)->folderId == 0);
	REQUIRE(store.AssignNote(read.entryId, folder.id).ok);
	store.Clear();
	REQUIRE(store.VisibleSnapshot().empty());
	REQUIRE(store.Folders().size() == 1);
	const auto next = store.ObserveRead(Identity(0x2000), "Next", "body", NoteBodyMode::ResolveStatic, Encounter());
	REQUIRE(next.entryId == read.entryId + 1);
	REQUIRE(store.Find(next.entryId)->firstReadSequence == 2);
}

TEST_CASE("individual deletion removes only the note")
{
	NoteStore store;
	const auto folder = store.CreateFolder(0, "Folder");
	const auto read = store.ObserveRead(Identity(), "Note", "body", NoteBodyMode::CachedResolved, Encounter());
	REQUIRE(store.AssignNote(read.entryId, folder.id).ok);
	REQUIRE(store.DeleteNote(read.entryId).ok);
	REQUIRE(store.VisibleSnapshot().empty());
	REQUIRE(store.Folders().size() == 1);
	REQUIRE(!store.DeleteNote(read.entryId).ok);
}

TEST_CASE("note v2 codec round-trips folder graph and assignments")
{
	std::vector<NoteFolder> folders{ { 1, 0, "Root", 1 }, { 2, 1, "Child", 1 }, { 3, 2, "Grandchild", 1 } };
	const auto bytes = NoteCosaveCodec::EncodeV2({ ReadRecord(7, 3) }, folders);
	std::vector<NoteRecord> records;
	std::vector<NoteFolder> decodedFolders;
	REQUIRE(NoteCosaveCodec::Decode(bytes, records, decodedFolders, 2));
	REQUIRE(records.size() == 1);
	REQUIRE(records[0].folderId == 3);
	REQUIRE(decodedFolders.size() == 3);
	REQUIRE(decodedFolders[2].parentFolderId == 2);
}

TEST_CASE("note v1 records load as Unfiled without folders")
{
	const auto bytes = NoteCosaveCodec::Encode({ ReadRecord(7, 0) });
	std::vector<NoteRecord> records;
	std::vector<NoteFolder> folders;
	REQUIRE(NoteCosaveCodec::Decode(bytes, records, folders, 1));
	REQUIRE(records.size() == 1);
	REQUIRE(records[0].folderId == 0);
	REQUIRE(folders.empty());
}

TEST_CASE("note v2 rejects missing parents and excessive depth")
{
	const std::vector<NoteFolder> missing{ { 1, 99, "Orphan", 1 } };
	const auto missingBytes = NoteCosaveCodec::EncodeV2({}, missing);
	std::vector<NoteRecord> records;
	std::vector<NoteFolder> folders;
	REQUIRE(!NoteCosaveCodec::Decode(missingBytes, records, folders, 2));

	const std::vector<NoteFolder> deep{ { 1, 0, "One", 1 }, { 2, 1, "Two", 1 }, { 3, 2, "Three", 1 }, { 4, 3, "Four", 1 } };
	const auto deepBytes = NoteCosaveCodec::EncodeV2({}, deep);
	REQUIRE(!NoteCosaveCodec::Decode(deepBytes, records, folders, 2));
}

TEST_CASE("note v2 rejects duplicate folder IDs names and bad note assignments")
{
	const std::vector<NoteFolder> duplicateIds{ { 1, 0, "One", 1 }, { 1, 0, "Two", 1 } };
	const auto idBytes = NoteCosaveCodec::EncodeV2({}, duplicateIds);
	std::vector<NoteRecord> records;
	std::vector<NoteFolder> folders;
	REQUIRE(!NoteCosaveCodec::Decode(idBytes, records, folders, 2));

	const std::vector<NoteFolder> duplicateNames{ { 1, 0, "Same", 1 }, { 2, 0, "same", 1 } };
	const auto nameBytes = NoteCosaveCodec::EncodeV2({}, duplicateNames);
	REQUIRE(!NoteCosaveCodec::Decode(nameBytes, records, folders, 2));

	const std::vector<NoteFolder> root{ { 1, 0, "Root", 1 } };
	const auto assignmentBytes = NoteCosaveCodec::EncodeV2({ ReadRecord(7, 99) }, root);
	REQUIRE(!NoteCosaveCodec::Decode(assignmentBytes, records, folders, 2));
}

TEST_CASE("note v2 rejects trailing data")
{
	auto bytes = NoteCosaveCodec::EncodeV2({}, {});
	bytes.push_back(std::byte{ 0xAA });
	std::vector<NoteRecord> records;
	std::vector<NoteFolder> folders;
	REQUIRE(!NoteCosaveCodec::Decode(bytes, records, folders, 2));
}

TEST_CASE("note tree payload carries folders assignments and operation feedback")
{
	const std::vector<NoteFolder> folders{ { 1, 0, "Root", 1 } };
	const std::vector<NoteListItem> notes{ { 7, "Note", 4, 1 } };
	const auto json = NotePayloadBuilder::BuildTree(folders, notes);
	REQUIRE(json.find("\"folders\":[{\"id\":1,\"parentId\":0,\"name\":\"Root\",\"ordinal\":1}]") != std::string::npos);
	REQUIRE(json.find("\"folderId\":1") != std::string::npos);
	const auto result = NotePayloadBuilder::BuildOperationResult({ true, 7, "Moved" });
	REQUIRE(result.find("\"ok\":true") != std::string::npos);
	REQUIRE(result.find("\"message\":\"Moved\"") != std::string::npos);
}
