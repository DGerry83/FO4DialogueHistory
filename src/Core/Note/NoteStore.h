#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "NoteEntry.h"

namespace F4DH::Core
{
	class NoteStore
	{
	public:
		[[nodiscard]] NoteChange ObservePickup(
			NoteIdentity a_identity,
			std::string a_originalTitle,
			const EncounterMetadata& a_encounter);
		[[nodiscard]] NoteChange ObserveRead(
			NoteIdentity a_identity,
			std::string a_originalTitle,
			const std::string& a_plainBody,
			NoteBodyMode a_bodyMode,
			const EncounterMetadata& a_encounter);

		void Clear();
		void ReplaceAll(std::vector<NoteRecord> a_records, std::vector<NoteFolder> a_folders = {});
		void SetGenericTitles(std::vector<std::string> a_titles) { _genericTitles = std::move(a_titles); }

		[[nodiscard]] NoteOperation CreateFolder(std::uint64_t a_parentFolderId, std::string a_name);
		[[nodiscard]] NoteOperation AssignNote(std::uint64_t a_entryId, std::uint64_t a_folderId);
		[[nodiscard]] NoteOperation DeleteNote(std::uint64_t a_entryId);

		[[nodiscard]] std::size_t Size() const;
		[[nodiscard]] std::vector<NoteRecord> Snapshot() const;
		[[nodiscard]] std::vector<NoteRecord> VisibleSnapshot() const;
		[[nodiscard]] std::vector<NoteFolder> Folders() const;
		[[nodiscard]] std::optional<NoteRecord> Find(std::uint64_t a_entryId) const;
		[[nodiscard]] std::optional<NoteFolder> FindFolder(std::uint64_t a_folderId) const;

	private:
		[[nodiscard]] std::int32_t FindMatchLocked(const NoteIdentity& a_identity) const;
		[[nodiscard]] std::int32_t FindSyntheticContentMatchLocked(
			const NoteIdentity& a_identity, std::string_view a_title, std::string_view a_body) const;
		[[nodiscard]] std::int32_t FindFolderLocked(std::uint64_t a_folderId) const;
		[[nodiscard]] std::uint32_t FolderDepthLocked(std::uint64_t a_folderId) const;
		void AssignNewIdentityLocked(NoteRecord& a_record);

		std::vector<NoteRecord> _records;
		std::vector<NoteFolder> _folders;
		std::vector<std::string> _genericTitles{ "Note", "Letter", "Ransom Note" };
		std::uint64_t _nextFolderId{ 1 };
		std::uint64_t _nextEntryId{ 1 };
		std::uint64_t _nextReadSequence{ 1 };
		mutable std::mutex _mutex;
	};
}
