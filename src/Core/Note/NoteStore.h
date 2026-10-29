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
		void ReplaceAll(std::vector<NoteRecord> a_records);
		void SetGenericTitles(std::vector<std::string> a_titles) { _genericTitles = std::move(a_titles); }

		[[nodiscard]] std::size_t Size() const;
		[[nodiscard]] std::vector<NoteRecord> Snapshot() const;
		[[nodiscard]] std::vector<NoteRecord> VisibleSnapshot() const;
		[[nodiscard]] std::optional<NoteRecord> Find(std::uint64_t a_entryId) const;

	private:
		[[nodiscard]] std::int32_t FindMatchLocked(const NoteIdentity& a_identity) const;
		[[nodiscard]] std::int32_t FindSyntheticContentMatchLocked(
			const NoteIdentity& a_identity, std::string_view a_title, std::string_view a_body) const;
		void AssignNewIdentityLocked(NoteRecord& a_record);

		std::vector<NoteRecord> _records;
		std::vector<std::string> _genericTitles{ "Note", "Letter", "Ransom Note" };
		std::uint64_t _nextEntryId{ 1 };
		std::uint64_t _nextReadSequence{ 1 };
		mutable std::mutex _mutex;
	};
}
