#pragma once

#include <cstdint>

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
	}
}
