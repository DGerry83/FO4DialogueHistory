#include "NoteCosaveCodec.h"

#include "NoteFolderRules.h"

#include <algorithm>
#include <string>

namespace
{
	void WriteU8(std::vector<std::byte>& a_out, std::uint8_t a_value)
	{
		a_out.push_back(std::byte{ a_value });
	}

	void WriteU32(std::vector<std::byte>& a_out, std::uint32_t a_value)
	{
		for (unsigned shift = 0; shift < 32; shift += 8) {
			a_out.push_back(static_cast<std::byte>((a_value >> shift) & 0xFF));
		}
	}

	void WriteU64(std::vector<std::byte>& a_out, std::uint64_t a_value)
	{
		for (unsigned shift = 0; shift < 64; shift += 8) {
			a_out.push_back(static_cast<std::byte>((a_value >> shift) & 0xFF));
		}
	}

	void WriteString(std::vector<std::byte>& a_out, const std::string& a_value)
	{
		WriteU32(a_out, static_cast<std::uint32_t>(a_value.size()));
		a_out.insert(a_out.end(), reinterpret_cast<const std::byte*>(a_value.data()),
			reinterpret_cast<const std::byte*>(a_value.data() + a_value.size()));
	}

	class Reader
	{
	public:
		explicit Reader(std::span<const std::byte> a_data) : _data(a_data) {}

		[[nodiscard]] bool U8(std::uint8_t& a_out)
		{
			if (_pos >= _data.size()) return false;
			a_out = static_cast<std::uint8_t>(_data[_pos++]);
			return true;
		}

		[[nodiscard]] bool U32(std::uint32_t& a_out)
		{
			if (_data.size() - _pos < 4) return false;
			std::uint32_t value = 0;
			for (unsigned shift = 0; shift < 32; shift += 8) {
				value |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(_data[_pos++])) << shift;
			}
			a_out = value;
			return true;
		}

		[[nodiscard]] bool U64(std::uint64_t& a_out)
		{
			if (_data.size() - _pos < 8) return false;
			std::uint64_t value = 0;
			for (unsigned shift = 0; shift < 64; shift += 8) {
				value |= static_cast<std::uint64_t>(static_cast<std::uint8_t>(_data[_pos++])) << shift;
			}
			a_out = value;
			return true;
		}

		[[nodiscard]] bool String(std::string& a_out)
		{
			std::uint32_t length = 0;
			if (!U32(length) || length > F4DH::Core::NoteCosaveCodec::kMaxStringBytes || length > _data.size() - _pos) {
				return false;
			}
			a_out.assign(reinterpret_cast<const char*>(_data.data() + _pos), length);
			_pos += length;
			return true;
		}

		[[nodiscard]] bool AtEnd() const { return _pos == _data.size(); }

	private:
		std::span<const std::byte> _data;
		std::size_t _pos{ 0 };
	};

	bool ValidString(const std::string& a_value)
	{
		return a_value.size() <= F4DH::Core::NoteCosaveCodec::kMaxStringBytes;
	}
}

namespace F4DH::Core
{
	std::vector<std::byte> NoteCosaveCodec::Encode(const std::vector<NoteRecord>& a_records)
	{
		if (a_records.size() > kMaxNotes) return {};
		std::vector<std::byte> out;
		WriteU32(out, static_cast<std::uint32_t>(a_records.size()));
		for (const auto& record : a_records) {
			if (!ValidString(record.title) || !ValidString(record.identity.pluginName) || !ValidString(record.cachedBody) ||
				(record.firstEncounter && !ValidString(record.firstEncounter->location))) {
				return {};
			}
			WriteU64(out, record.identity.entryId);
			WriteU64(out, record.firstReadSequence);
			WriteU8(out, static_cast<std::uint8_t>(record.identity.kind));
			WriteU8(out, static_cast<std::uint8_t>(record.state));
			WriteU8(out, static_cast<std::uint8_t>(record.bodyMode));
			WriteU32(out, record.identity.baseFormID);
			WriteU32(out, record.identity.sourceRefFormID);
			WriteU32(out, record.identity.localBaseFormID);
			WriteU32(out, record.identity.instanceOrdinal);
			WriteU8(out, record.firstEncounter ? 1 : 0);
			if (record.firstEncounter) {
				const auto& first = *record.firstEncounter;
				WriteU8(out, static_cast<std::uint8_t>(first.kind));
				WriteU32(out, static_cast<std::uint32_t>(first.timestamp.year));
				WriteU8(out, first.timestamp.month);
				WriteU8(out, first.timestamp.day);
				WriteU8(out, first.timestamp.hour);
				WriteU8(out, first.timestamp.minute);
			}
			WriteString(out, record.title);
			WriteString(out, record.identity.pluginName);
			WriteString(out, record.firstEncounter ? record.firstEncounter->location : std::string{});
			WriteString(out, record.cachedBody);
		}
		return out;
	}

	std::vector<std::byte> NoteCosaveCodec::EncodeV2(const std::vector<NoteRecord>& a_records, const std::vector<NoteFolder>& a_folders)
	{
		if (a_records.size() > kMaxNotes || a_folders.size() > kMaxFolders) return {};
		auto out = Encode(a_records);
		if (out.empty() && !a_records.empty()) return {};
		for (const auto& record : a_records) WriteU64(out, record.folderId);
		WriteU32(out, static_cast<std::uint32_t>(a_folders.size()));
		for (const auto& folder : a_folders) {
			if (!ValidString(folder.name) || NoteFolderRules::NormalizeName(folder.name) != folder.name) return {};
			WriteU64(out, folder.folderId);
			WriteU64(out, folder.parentFolderId);
			WriteU32(out, folder.ordinal);
			WriteString(out, folder.name);
		}
		return out;
	}

	bool NoteCosaveCodec::Decode(std::span<const std::byte> a_data, std::vector<NoteRecord>& a_outRecords, std::uint32_t a_version)
	{
		std::vector<NoteFolder> ignored;
		return Decode(a_data, a_outRecords, ignored, a_version);
	}

	bool NoteCosaveCodec::Decode(std::span<const std::byte> a_data, std::vector<NoteRecord>& a_outRecords, std::vector<NoteFolder>& a_outFolders, std::uint32_t a_version)
	{
		a_outRecords.clear();
		a_outFolders.clear();
		if (a_version != 1 && a_version != 2) return false;

		Reader reader(a_data);
		std::uint32_t count = 0;
		if (!reader.U32(count) || count > kMaxNotes) return false;
		std::vector<NoteRecord> decoded;
		for (std::uint32_t i = 0; i < count; ++i) {
			NoteRecord record;
			std::uint8_t identityKind = 0, state = 0, bodyMode = 0, hasEncounter = 0;
			std::uint32_t year = 0;
			if (!reader.U64(record.identity.entryId) || !reader.U64(record.firstReadSequence) ||
				!reader.U8(identityKind) || !reader.U8(state) || !reader.U8(bodyMode) ||
				!reader.U32(record.identity.baseFormID) || !reader.U32(record.identity.sourceRefFormID) ||
				!reader.U32(record.identity.localBaseFormID) || !reader.U32(record.identity.instanceOrdinal) ||
				!reader.U8(hasEncounter)) {
				return false;
			}
			if (identityKind > 2 || state > 1 || bodyMode > 3 || hasEncounter > 1 || record.identity.entryId == 0) return false;
			record.identity.kind = static_cast<NoteIdentityKind>(identityKind);
			record.state = static_cast<NoteState>(state);
			record.bodyMode = static_cast<NoteBodyMode>(bodyMode);
			if ((record.state == NoteState::Read) != (record.firstReadSequence != 0)) return false;

			if (hasEncounter) {
				EncounterMetadata first;
				std::uint8_t kind = 0;
				if (!reader.U8(kind) || kind > 1 || !reader.U32(year) ||
					!reader.U8(first.timestamp.month) || !reader.U8(first.timestamp.day) ||
					!reader.U8(first.timestamp.hour) || !reader.U8(first.timestamp.minute)) {
					return false;
				}
				first.kind = static_cast<EncounterKind>(kind);
				first.timestamp.year = static_cast<std::int32_t>(year);
				record.firstEncounter = first;
			}
			std::string location;
			if (!reader.String(record.title) || !reader.String(record.identity.pluginName) ||
				!reader.String(location) || !reader.String(record.cachedBody)) {
				return false;
			}
			if (record.firstEncounter) record.firstEncounter->location = std::move(location);
			else if (!location.empty()) return false;
			if (a_version == 2 && !reader.U64(record.folderId)) return false;
			decoded.push_back(std::move(record));
		}

		if (a_version == 1) {
			if (!reader.AtEnd()) return false;
			a_outRecords = std::move(decoded);
			return true;
		}

		std::uint32_t folderCount = 0;
		if (!reader.U32(folderCount) || folderCount > kMaxFolders) return false;
		std::vector<NoteFolder> folders;
		for (std::uint32_t i = 0; i < folderCount; ++i) {
			NoteFolder folder;
			if (!reader.U64(folder.folderId) || !reader.U64(folder.parentFolderId) ||
				!reader.U32(folder.ordinal) || !reader.String(folder.name)) {
				return false;
			}
			if (folder.folderId == 0 || folder.ordinal == 0 || folder.name.empty() ||
				NoteFolderRules::NormalizeName(folder.name) != folder.name) {
				return false;
			}
			for (const auto& other : folders) {
				if (other.folderId == folder.folderId) return false;
				if (other.parentFolderId == folder.parentFolderId && NoteFolderRules::SameName(other.name, folder.name)) return false;
			}
			if (folder.parentFolderId != 0) {
				std::uint32_t depth = 1;
				auto parent = folder.parentFolderId;
				while (parent != 0) {
					const auto it = std::ranges::find_if(folders, [&](const NoteFolder& candidate) { return candidate.folderId == parent; });
					if (it == folders.end() || ++depth > 3) return false;
					parent = it->parentFolderId;
				}
			}
			folders.push_back(std::move(folder));
		}
		for (const auto& record : decoded) {
			if (record.folderId != 0 && std::ranges::find_if(folders, [&](const NoteFolder& folder) { return folder.folderId == record.folderId; }) == folders.end()) {
				return false;
			}
		}
		if (!reader.AtEnd()) return false;
		a_outRecords = std::move(decoded);
		a_outFolders = std::move(folders);
		return true;
	}
}
