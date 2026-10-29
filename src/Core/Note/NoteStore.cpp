#include "NoteStore.h"

#include "NoteTitleGenerator.h"

#include <algorithm>

namespace F4DH::Core
{
	NoteChange NoteStore::ObservePickup(
		NoteIdentity a_identity,
		std::string a_originalTitle,
		const EncounterMetadata& a_encounter)
	{
		const std::lock_guard lock(_mutex);
		if (const auto match = FindMatchLocked(a_identity); match >= 0) {
			auto& record = _records[static_cast<std::size_t>(match)];
			if (!record.firstEncounter) record.firstEncounter = a_encounter;
			return { record.identity.entryId, false };
		}

		NoteRecord record;
		record.identity = std::move(a_identity);
		record.title = std::move(a_originalTitle);
		record.state = NoteState::PendingPickup;
		record.bodyMode = NoteBodyMode::ResolveStatic;
		record.firstEncounter = a_encounter;
		AssignNewIdentityLocked(record);
		const auto id = record.identity.entryId;
		_records.push_back(std::move(record));
		return { id, false };
	}

	NoteChange NoteStore::ObserveRead(
		NoteIdentity a_identity,
		std::string a_originalTitle,
		const std::string& a_plainBody,
		NoteBodyMode a_bodyMode,
		const EncounterMetadata& a_encounter)
	{
		const std::lock_guard lock(_mutex);
		auto match = FindMatchLocked(a_identity);
		if (match < 0 && a_identity.kind == NoteIdentityKind::DynamicSynthetic) {
			match = FindSyntheticContentMatchLocked(a_identity, a_originalTitle, a_plainBody);
		}
		if (match >= 0) {
			auto& record = _records[static_cast<std::size_t>(match)];
			if (record.state == NoteState::Read) return { record.identity.entryId, false };

			record.state = NoteState::Read;
			record.bodyMode = a_bodyMode;
			record.cachedBody = a_bodyMode == NoteBodyMode::CachedResolved ? a_plainBody : std::string{};
			record.firstReadSequence = _nextReadSequence++;
			if (!record.firstEncounter) record.firstEncounter = a_encounter;

			std::vector<std::string> titles;
			for (const auto& other : _records) {
				if (other.state == NoteState::Read && other.identity.entryId != record.identity.entryId) {
					titles.push_back(other.title);
				}
			}
			record.title = NoteTitleGenerator::Generate(a_originalTitle, a_plainBody, titles, _genericTitles);
			return { record.identity.entryId, true };
		}

		NoteRecord record;
		record.identity = std::move(a_identity);
		record.state = NoteState::Read;
		record.bodyMode = a_bodyMode;
		record.cachedBody = a_bodyMode == NoteBodyMode::CachedResolved ? a_plainBody : std::string{};
		record.firstEncounter = a_encounter;
		record.firstReadSequence = _nextReadSequence++;
		AssignNewIdentityLocked(record);

		std::vector<std::string> titles;
		for (const auto& other : _records) {
			if (other.state == NoteState::Read) titles.push_back(other.title);
		}
		record.title = NoteTitleGenerator::Generate(a_originalTitle, a_plainBody, titles, _genericTitles);
		const auto id = record.identity.entryId;
		_records.push_back(std::move(record));
		return { id, true };
	}

	void NoteStore::Clear()
	{
		const std::lock_guard lock(_mutex);
		_records.clear();
		_nextEntryId = 1;
		_nextReadSequence = 1;
	}

	void NoteStore::ReplaceAll(std::vector<NoteRecord> a_records)
	{
		const std::lock_guard lock(_mutex);
		_records = std::move(a_records);
		_nextEntryId = 1;
		_nextReadSequence = 1;
		for (const auto& record : _records) {
			_nextEntryId = std::max(_nextEntryId, record.identity.entryId + 1);
			_nextReadSequence = std::max(_nextReadSequence, record.firstReadSequence + 1);
		}
	}

	std::size_t NoteStore::Size() const
	{
		const std::lock_guard lock(_mutex);
		return _records.size();
	}

	std::vector<NoteRecord> NoteStore::Snapshot() const
	{
		const std::lock_guard lock(_mutex);
		return _records;
	}

	std::vector<NoteRecord> NoteStore::VisibleSnapshot() const
	{
		const std::lock_guard lock(_mutex);
		std::vector<NoteRecord> out;
		for (const auto& record : _records) {
			if (record.state == NoteState::Read) out.push_back(record);
		}
		return out;
	}

	std::optional<NoteRecord> NoteStore::Find(std::uint64_t a_entryId) const
	{
		const std::lock_guard lock(_mutex);
		const auto it = std::ranges::find_if(_records, [&](const NoteRecord& record) {
			return record.identity.entryId == a_entryId && record.state == NoteState::Read;
		});
		return it == _records.end() ? std::nullopt : std::optional<NoteRecord>(*it);
	}

	std::int32_t NoteStore::FindMatchLocked(const NoteIdentity& a_identity) const
	{
		for (std::size_t i = 0; i < _records.size(); ++i) {
			const auto& other = _records[i].identity;
			if (other.baseFormID != a_identity.baseFormID || other.kind != a_identity.kind) continue;
			if (a_identity.kind == NoteIdentityKind::StaticBase) return static_cast<std::int32_t>(i);
			if (a_identity.sourceRefFormID != 0 && other.sourceRefFormID == a_identity.sourceRefFormID) {
				return static_cast<std::int32_t>(i);
			}
			if (a_identity.runtimeUniqueID != 0 && other.runtimeUniqueID == a_identity.runtimeUniqueID) {
				return static_cast<std::int32_t>(i);
			}
		}
		return -1;
	}

	std::int32_t NoteStore::FindSyntheticContentMatchLocked(
		const NoteIdentity& a_identity, std::string_view, std::string_view a_body) const
	{
		if (a_identity.sourceRefFormID != 0 || a_identity.runtimeUniqueID != 0) return -1;
		for (std::size_t i = 0; i < _records.size(); ++i) {
			const auto& record = _records[i];
			if (record.identity.kind == NoteIdentityKind::DynamicSynthetic &&
				record.identity.baseFormID == a_identity.baseFormID && record.cachedBody == a_body) {
				return static_cast<std::int32_t>(i);
			}
		}
		return -1;
	}

	void NoteStore::AssignNewIdentityLocked(NoteRecord& a_record)
	{
		a_record.identity.entryId = _nextEntryId++;
		if (a_record.identity.kind == NoteIdentityKind::DynamicSynthetic && a_record.identity.instanceOrdinal == 0) {
			std::uint32_t ordinal = 0;
			for (const auto& other : _records) {
				if (other.identity.kind == NoteIdentityKind::DynamicSynthetic &&
					other.identity.baseFormID == a_record.identity.baseFormID) {
					ordinal = std::max(ordinal, other.identity.instanceOrdinal);
				}
			}
			a_record.identity.instanceOrdinal = ordinal + 1;
		}
	}
}
