#include "CosaveStore.h"

#include <vector>

#include "Core/Dialogue/CosaveCodec.h"
#include "Core/Dialogue/DialogueBuffer.h"
#include "Core/Message/MessageBuffer.h"
#include "Core/Message/MessageCosaveCodec.h"
#include "Core/Note/NoteCosaveCodec.h"
#include "Core/Note/NoteStore.h"

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
	constexpr std::uint32_t kMessageRecordVersion = 1;
	constexpr std::uint32_t kNoteRecordType = 'NOTE';
	constexpr std::uint32_t kNoteRecordVersion = 1;

	F4DH::Core::DialogueBuffer* g_buffer{ nullptr };
	F4DH::Core::MessageBuffer*  g_messageBuffer{ nullptr };
	F4DH::Core::NoteStore*      g_noteStore{ nullptr };

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
		if (g_noteStore) {
			const auto payload = F4DH::Core::NoteCosaveCodec::Encode(g_noteStore->Snapshot());
			a_ser->WriteRecord(kNoteRecordType, kNoteRecordVersion, std::span<const std::byte>(payload.data(), payload.size()));
		}
	}

	void LoadCallback(const F4SE::SerializationInterface* a_ser)
	{
		auto* buffer = g_buffer;
		auto* messageBuffer = g_messageBuffer;
		auto* noteStore = g_noteStore;
		if (buffer) buffer->Clear();
		if (messageBuffer) messageBuffer->Clear();
		if (noteStore) noteStore->Clear();

		std::uint32_t type = 0;
		std::uint32_t version = 0;
		std::uint32_t size = 0;
		while (a_ser->GetNextRecordInfo(type, version, size)) {
			const bool isDialogue = type == kDialogueRecordType && (version == 1 || version == 2);
			const bool isMessage = type == kMessageRecordType && version == kMessageRecordVersion;
			const bool isNote = type == kNoteRecordType && version == kNoteRecordVersion;
			if (!isDialogue && !isMessage && !isNote) continue;
			if ((isDialogue && !buffer) || (isMessage && !messageBuffer) || (isNote && !noteStore)) continue;

			std::vector<std::byte> payload(size);
			if (a_ser->ReadRecordData(payload.data(), size) != size) {
				REX::LogError("cosave: short read on {} record ({} bytes) — history load aborted",
					isDialogue ? "HIST" : isMessage ? "MSGS" : "NOTE", size);
				return;
			}

			if (isDialogue) {
				std::vector<F4DH::Core::DialogueLine> lines;
				if (!F4DH::Core::CosaveCodec::Decode(payload, lines, version)) {
					REX::LogError("cosave: malformed HIST record ({} bytes) — dialogue history stays cleared", size);
					continue;
				}
				for (auto& line : lines) buffer->Push(std::move(line));
				REX::LogInformation("cosave: restored {} dialogue lines", lines.size());
			} else if (isMessage) {
				std::vector<F4DH::Core::MessageLine> messages;
				if (!F4DH::Core::MessageCosaveCodec::Decode(payload, messages, version)) {
					REX::LogError("cosave: malformed MSGS record ({} bytes) — message history stays cleared", size);
					continue;
				}
				for (auto& message : messages) messageBuffer->Push(std::move(message));
				REX::LogInformation("cosave: restored {} message(s)", messages.size());
			} else {
				std::vector<F4DH::Core::NoteRecord> records;
				if (!F4DH::Core::NoteCosaveCodec::Decode(payload, records, version)) {
					REX::LogError("cosave: malformed NOTE record ({} bytes) — note history stays cleared", size);
					continue;
				}
				for (auto& record : records) {
					record.identity.baseFormID = a_ser->ResolveFormID(record.identity.baseFormID).value_or(0);
					record.identity.sourceRefFormID = record.identity.sourceRefFormID != 0 ?
						a_ser->ResolveFormID(record.identity.sourceRefFormID).value_or(0) : 0;
					record.identity.runtimeUniqueID = 0;
				}
				noteStore->ReplaceAll(std::move(records));
				REX::LogInformation("cosave: restored {} note record(s)", noteStore->Size());
			}
		}
	}

	void RevertCallback(const F4SE::SerializationInterface*)
	{
		if (g_buffer) {
			g_buffer->Clear();  // new game / revert — drop restored history
		}
		if (g_messageBuffer) g_messageBuffer->Clear();
		if (g_noteStore) g_noteStore->Clear();
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
		REX::LogInformation("cosave: persistence registered (unique ID 'DLGH', records 'HIST' v2 [loads v1+v2] + 'MSGS' v1 + 'NOTE' v1)");
	}

	void CosaveStore::Bind(Core::DialogueBuffer* a_buffer, Core::MessageBuffer* a_messageBuffer, Core::NoteStore* a_noteStore)
	{
		g_buffer = a_buffer;
		g_messageBuffer = a_messageBuffer;
		g_noteStore = a_noteStore;
	}
}
