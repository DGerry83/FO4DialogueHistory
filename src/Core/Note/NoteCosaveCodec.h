#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "NoteEntry.h"

namespace F4DH::Core
{
	class NoteCosaveCodec
	{
	public:
		static constexpr std::uint32_t kMaxNotes = 10000;
		static constexpr std::uint32_t kMaxFolders = 256;
		static constexpr std::uint32_t kMaxStringBytes = 1024 * 1024;

		[[nodiscard]] static std::vector<std::byte> Encode(const std::vector<NoteRecord>& a_records);
		[[nodiscard]] static std::vector<std::byte> EncodeV2(const std::vector<NoteRecord>& a_records, const std::vector<NoteFolder>& a_folders);
		[[nodiscard]] static bool Decode(std::span<const std::byte> a_data, std::vector<NoteRecord>& a_outRecords, std::uint32_t a_version);
		[[nodiscard]] static bool Decode(std::span<const std::byte> a_data, std::vector<NoteRecord>& a_outRecords, std::vector<NoteFolder>& a_outFolders, std::uint32_t a_version);
	};
}
