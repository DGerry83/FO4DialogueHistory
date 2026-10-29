#pragma once

#include <cstdint>
#include <string>

namespace F4DH::Core
{
	class ILogger;
}

namespace F4DH::Infrastructure::Notes
{
	// Value-only copies of the native event payloads (ruling D5): every field
	// is POD, captured in NoteEventSink::ProcessEvent without engine calls,
	// then handed to the game-thread task. No pointers, no NiPointer, no
	// engine types — these never outlive the dispatch and are never persisted.
	struct ReadObservation
	{
		std::uint64_t seq;             // per-sink monotonic counter (ordering evidence)
		std::uint32_t fromThreadId;    // thread that ran ProcessEvent (thread-context gate)
		std::uint32_t bookRefFormID;   // 0 = null bookRef (identity gate: world vs inventory read)
		std::uint32_t baseFormID;      // TESBookReadEvent::baseFormID
		std::uint16_t uniqueID;        // TESBookReadEvent::uniqueID (transient 16-bit stack id)
	};

	struct TransferObservation
	{
		std::uint64_t seq;
		std::uint32_t fromThreadId;
		std::uint32_t oldContainerFormID;
		std::uint32_t newContainerFormID;
		std::uint32_t baseItemFormID;
		std::uint32_t itemCount;
		std::uint32_t itemRefFormID;   // 0 = no persistent item reference
		std::uint16_t uniqueID;
	};

	// Value-only payload from the OG BookMenu::PrepareBookText detour
	// (BookMenuTextHook). The prepared body was copied on the hook path under
	// a 64 KiB cap (truncated records it); every field here is plain data.
	struct PreparedTextObservation
	{
		std::uint64_t              seq;          // independent bookText sequence
		std::uint32_t              fromThreadId; // thread that ran the thunk
		std::uint32_t              bookFormID;   // target-book slot raw formID (0 = null/unreadable)
		std::uint32_t              refFormID;    // target-reference slot raw formID (0 = null/unreadable)
		std::uint64_t              finalLength;  // NUL-terminated length over the bounded span
		bool                       truncated;    // finalLength exceeded the 64 KiB copy cap
		std::string                body;         // <= 64 KiB, control characters escaped at log time
	};

	// M1 game-thread probes. Every function here runs exclusively on the game
	// thread (dispatched by NoteEventSink through the F4SE task interface) and
	// performs all engine access: form resolution, magazine-marker probes
	// (OBJ_BOOK flags / keywords / model / script-metadata availability),
	// both TESDescription candidates (inherited and item-card; the static
	// form-description route and the reference-parent route are permanently
	// disabled — OG freeze evidence, 2026-10-28_Bug_NoteReadFreeze), and
	// Calendar/player location capture. Logging-only; no state is retained
	// between calls.
	namespace NoteRecordResolver
	{
		void LogReadObservation(const ReadObservation& a_obs, Core::ILogger& a_log);
		void LogTransferObservation(const TransferObservation& a_obs, Core::ILogger& a_log);
		void LogPreparedBookText(PreparedTextObservation a_obs, Core::ILogger& a_log);
	}
}
