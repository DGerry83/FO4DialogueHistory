#include "CosaveStore.h"

#include <vector>

#include "Core/Dialogue/CosaveCodec.h"
#include "Core/Dialogue/DialogueBuffer.h"

#include "F4SE/API.hpp"
#include "F4SE/SerializationInterface.hpp"

#include "REX/Log.hpp"

namespace
{
	// Fourcc unique ID and record header per M3 gates. Multi-character
	// constants, matching the fourcc spelling byte-for-byte in the gates.
	constexpr std::uint32_t kUniqueID = 'DLGH';
	constexpr std::uint32_t kRecordType = 'HIST';
	constexpr std::uint32_t kRecordVersion = 1;

	F4DH::Core::DialogueBuffer* g_buffer{ nullptr };  // bound at game-data-ready

	void SaveCallback(const F4SE::SerializationInterface* a_ser)
	{
		if (!g_buffer) {
			return;  // graph not built yet — nothing to persist
		}

		const auto payload = F4DH::Core::CosaveCodec::Encode(g_buffer->Snapshot());
		// One record carries the whole buffer payload. WriteRecord hard-aborts
		// (REX::Fail) internally on failure, so a save can never silently
		// half-persist.
		a_ser->WriteRecord(kRecordType, kRecordVersion, std::span<const std::byte>(payload.data(), payload.size()));
	}

	void LoadCallback(const F4SE::SerializationInterface* a_ser)
	{
		auto* buffer = g_buffer;
		if (buffer) {
			buffer->Clear();  // rebuild from scratch below
		}

		std::uint32_t type = 0;
		std::uint32_t version = 0;
		std::uint32_t size = 0;
		while (a_ser->GetNextRecordInfo(type, version, size)) {
			if (type != kRecordType || version != kRecordVersion) {
				continue;  // unknown record — skip without reading; no abort
			}
			if (!buffer) {
				continue;  // graph not built yet — discard the record unread
			}

			// Read EXACTLY the record size — a short read hard-aborts
			// (REX::Fail) inside F4SE, so the size math here must be exact.
			std::vector<std::byte> payload(size);
			if (a_ser->ReadRecordData(payload.data(), size) != size) {
				REX::LogError("cosave: short read on HIST record ({} bytes) — history cleared", size);
				return;
			}

			std::vector<F4DH::Core::DialogueLine> lines;
			if (!F4DH::Core::CosaveCodec::Decode(payload, lines)) {
				REX::LogError("cosave: malformed HIST record ({} bytes) — history cleared", size);
				return;  // graceful rejection: buffer stays cleared
			}

			for (auto& line : lines) {
				buffer->Push(std::move(line));  // ring capacity evicts the oldest
			}
			REX::LogInformation("cosave: restored {} dialogue lines", lines.size());
			return;  // one HIST record per save
		}
	}

	void RevertCallback(const F4SE::SerializationInterface*)
	{
		if (g_buffer) {
			g_buffer->Clear();  // new game / revert — drop restored history
		}
	}
}

namespace F4DH::Infrastructure
{
	void CosaveStore::Install()
	{
		const auto ser = F4SE::GetSerializationInterface();
		ser->SetUniqueID(kUniqueID);
		ser->SetSaveCallback(SaveCallback);
		ser->SetLoadCallback(LoadCallback);
		ser->SetRevertCallback(RevertCallback);
		REX::LogInformation("cosave: persistence registered (unique ID 'DLGH', record 'HIST' v1)");
	}

	void CosaveStore::Bind(Core::DialogueBuffer* a_buffer)
	{
		g_buffer = a_buffer;
	}
}
