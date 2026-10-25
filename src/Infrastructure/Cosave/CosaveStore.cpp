#include "CosaveStore.h"

#include <vector>

#include "Core/Dialogue/CosaveCodec.h"
#include "Core/Dialogue/DialogueBuffer.h"
#include "Core/Message/MessageBuffer.h"
#include "Core/Message/MessageCosaveCodec.h"

#include "F4SE/API.hpp"
#include "F4SE/SerializationInterface.hpp"

#include "REX/Log.hpp"

namespace
{
	// Fourcc unique ID and record headers per M3 gates + Message History
	// v1.3.0. Multi-character constants, matching the fourcc spelling
	// byte-for-byte in the gates.
	constexpr std::uint32_t kUniqueID = 'DLGH';
	constexpr std::uint32_t kDialogueRecordType = 'HIST';
	constexpr std::uint32_t kDialogueRecordVersion = 2;  // v2 = +questType per line; loader also accepts v1
	constexpr std::uint32_t kMessageRecordType = 'MSGS';
	constexpr std::uint32_t kMessageRecordVersion = 1;   // v1 = count + length-bounded UTF-8 strings

	F4DH::Core::DialogueBuffer* g_buffer{ nullptr };         // bound at game-data-ready
	F4DH::Core::MessageBuffer*  g_messageBuffer{ nullptr };  // bound at game-data-ready

	void SaveCallback(const F4SE::SerializationInterface* a_ser)
	{
		// Each history is written independently: one record carries one
		// buffer's payload, so a future partial failure can never
		// half-persist across histories. WriteRecord hard-aborts (REX::Fail)
		// internally on failure, so a save can never silently half-persist
		// within a record either.
		if (g_buffer) {
			const auto payload = F4DH::Core::CosaveCodec::Encode(g_buffer->Snapshot());
			a_ser->WriteRecord(kDialogueRecordType, kDialogueRecordVersion, std::span<const std::byte>(payload.data(), payload.size()));
		}
		if (g_messageBuffer) {
			const auto payload = F4DH::Core::MessageCosaveCodec::Encode(g_messageBuffer->Snapshot());
			a_ser->WriteRecord(kMessageRecordType, kMessageRecordVersion, std::span<const std::byte>(payload.data(), payload.size()));
		}
	}

	void LoadCallback(const F4SE::SerializationInterface* a_ser)
	{
		// Load replaces: both buffers are cleared up front and rebuilt only
		// from their own recognized record. A pre-1.3.0 save has no MSGS
		// record — the message buffer simply stays empty.
		auto* buffer = g_buffer;
		auto* messageBuffer = g_messageBuffer;
		if (buffer) {
			buffer->Clear();
		}
		if (messageBuffer) {
			messageBuffer->Clear();
		}

		std::uint32_t type = 0;
		std::uint32_t version = 0;
		std::uint32_t size = 0;
		while (a_ser->GetNextRecordInfo(type, version, size)) {
			const bool isDialogue = type == kDialogueRecordType && (version == 1 || version == 2);
			const bool isMessage = type == kMessageRecordType && version == kMessageRecordVersion;
			if (!isDialogue && !isMessage) {
				continue;  // unknown record/version — skip without reading; no abort
			}
			if ((isDialogue && !buffer) || (isMessage && !messageBuffer)) {
				continue;  // graph not built yet — discard the record unread
			}

			// Read EXACTLY the record size — a short read hard-aborts
			// (REX::Fail) inside F4SE, so the size math here must be exact.
			std::vector<std::byte> payload(size);
			if (a_ser->ReadRecordData(payload.data(), size) != size) {
				REX::LogError("cosave: short read on {} record ({} bytes) — history load aborted", isDialogue ? "HIST" : "MSGS", size);
				return;  // catastrophic stream failure — nothing sane remains to read
			}

			if (isDialogue) {
				std::vector<F4DH::Core::DialogueLine> lines;
				if (!F4DH::Core::CosaveCodec::Decode(payload, lines, version)) {
					REX::LogError("cosave: malformed HIST record ({} bytes) — dialogue history stays cleared", size);
					continue;  // fail closed for dialogue only; keep scanning (MSGS may still be valid)
				}
				for (auto& line : lines) {
					buffer->Push(std::move(line));  // ring capacity evicts the oldest
				}
				REX::LogInformation("cosave: restored {} dialogue lines", lines.size());
			} else {
				std::vector<F4DH::Core::MessageLine> messages;
				if (!F4DH::Core::MessageCosaveCodec::Decode(payload, messages, version)) {
					REX::LogError("cosave: malformed MSGS record ({} bytes) — message history stays cleared", size);
					continue;  // fail closed for messages only; HIST may still be valid
				}
				for (auto& message : messages) {
					messageBuffer->Push(std::move(message));  // capacity evicts the oldest
				}
				REX::LogInformation("cosave: restored {} message(s)", messages.size());
			}
		}
	}

	void RevertCallback(const F4SE::SerializationInterface*)
	{
		if (g_buffer) {
			g_buffer->Clear();  // new game / revert — drop restored history
		}
		if (g_messageBuffer) {
			g_messageBuffer->Clear();
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
		REX::LogInformation("cosave: persistence registered (unique ID 'DLGH', records 'HIST' v2 [loads v1+v2] + 'MSGS' v1)");
	}

	void CosaveStore::Bind(Core::DialogueBuffer* a_buffer, Core::MessageBuffer* a_messageBuffer)
	{
		g_buffer = a_buffer;
		g_messageBuffer = a_messageBuffer;
	}
}
