#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "Application/Capture/INoteSink.h"
#include "Application/View/INoteBodySource.h"

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

	// Game-thread native resolution. Value-returning paths feed Application
	// capture; logging paths remain available for diagnostics. Static form and
	// reference-parent description routes stay disabled.
	namespace NoteRecordResolver
	{
		void LogReadObservation(const ReadObservation& a_obs, Core::ILogger& a_log);
		void LogTransferObservation(const TransferObservation& a_obs, Core::ILogger& a_log);
		void LogPreparedBookText(PreparedTextObservation a_obs, Core::ILogger& a_log);

		[[nodiscard]] std::optional<Application::NotePickupEvent> ResolveTransferObservation(const TransferObservation& a_obs, Core::ILogger& a_log);
		[[nodiscard]] std::optional<Application::NoteReadEvent> ResolvePreparedText(const PreparedTextObservation& a_obs, Core::ILogger& a_log);

		class BodySource final : public Application::INoteBodySource
		{
		public:
			[[nodiscard]] Core::NoteBodyResult ResolveBody(const Core::NoteRecord& record) override;
		};
	}
}
