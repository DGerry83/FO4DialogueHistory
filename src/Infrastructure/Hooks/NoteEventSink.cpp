#include "NoteEventSink.h"

#include "Core/ILogger.h"
#include "Application/Capture/INoteSink.h"
#include "Infrastructure/Notes/NoteRecordResolver.h"

#include "F4SE/API.hpp"

#include "RE/T/TESBookReadEvent.hpp"
#include "RE/T/TESContainerChangedEvent.hpp"

#include <Windows.h>

#include <atomic>
#include <format>

// M1 Note History diagnostic prototype (see header). Delivery discipline
// (native-interop checklist): ProcessEvent performs no engine calls and no
// file I/O on the dispatch thread — it copies bounded POD fields, records the
// originating thread ID, and queues one F4SE task per event. The only
// delivery-path allocation is the task queue node + std::function target
// (user-action event rate — book opens and item transfers are not per-frame
// traffic); all engine access and log formatting happen on the game thread.
// The bookRef NiPointer is never copied or held: only its formID is read,
// null-safe, while the event argument keeps the object alive for the call.

namespace
{
	// Per-sink monotonic sequence for cross-event ordering evidence (G1:
	// read-then-pickup vs pickup-then-read in one transaction). Atomic because
	// the dispatch thread is a gate unknown until runtime evidence exists.
	std::atomic<std::uint64_t> g_seq{ 0 };

	std::uint64_t NextSeq() noexcept
	{
		return g_seq.fetch_add(1, std::memory_order_relaxed) + 1;
	}

	// Install is reachable once; mirrors HudMessageSink's idempotent install.
	bool g_installed{ false };

	// Task interface resolved at install and cached: the delivery path then
	// dereferences this pointer without touching the F4SE API table. Only ever
	// non-null while the sinks are registered (Install is the sole writer).
	const F4SE::TaskInterface* g_taskInterface{ nullptr };
}

namespace F4DH::Infrastructure
{
	NoteEventSink::NoteEventSink(Core::ILogger& a_log, bool a_verbose, Application::INoteSink* a_sink) :
		_log(a_log),
		_verbose(a_verbose),
		_sink(a_sink)
	{}

	RE::BSEventNotifyControl NoteEventSink::ProcessEvent(const RE::TESBookReadEvent& a_event, RE::BSTEventSource<RE::TESBookReadEvent>*)
	{
		if (!_verbose && !_sink) return RE::BSEventNotifyControl::kContinue;

		Notes::ReadObservation obs;
		obs.seq = NextSeq();
		obs.fromThreadId = ::GetCurrentThreadId();
		obs.bookRefFormID = a_event.bookRef ? a_event.bookRef->formID : 0;
		obs.baseFormID = a_event.baseFormID;
		obs.uniqueID = a_event.uniqueID;

		g_taskInterface->AddTask([this, obs] {
			Notes::NoteRecordResolver::CaptureReadTitleHint(obs);
			if (_verbose) Notes::NoteRecordResolver::LogReadObservation(obs, _log);
		});
		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl NoteEventSink::ProcessEvent(const RE::TESContainerChangedEvent& a_event, RE::BSTEventSource<RE::TESContainerChangedEvent>*)
	{
		if (!_verbose && !_sink) return RE::BSEventNotifyControl::kContinue;

		Notes::TransferObservation obs;
		obs.seq = NextSeq();
		obs.fromThreadId = ::GetCurrentThreadId();
		obs.oldContainerFormID = a_event.oldContainerFormID;
		obs.newContainerFormID = a_event.newContainerFormID;
		obs.baseItemFormID = a_event.baseItemFormID;
		obs.itemCount = a_event.itemCount;
		obs.itemRefFormID = a_event.itemRefFormID;
		obs.uniqueID = a_event.uniqueID;

		g_taskInterface->AddTask([this, obs] {
			if (_sink) {
				if (auto event = Notes::NoteRecordResolver::ResolveTransferObservation(obs, _log)) {
					_sink->OnNotePickup(*event);
				}
			}
			if (_verbose) Notes::NoteRecordResolver::LogTransferObservation(obs, _log);
		});
		return RE::BSEventNotifyControl::kContinue;
	}

	bool NoteEventSink::Install(Core::ILogger& a_log, bool a_verbose, Application::INoteSink* a_sink)
	{
		if (g_installed) {
			return true;
		}

		// Install-time resource accounting: each failure path leaves zero
		// subscriptions behind. The task interface is required for game-thread
		// marshaling — GetTaskInterface returns REX::NotNull, so a null check
		// is unnecessary (the contract asserts non-null after F4SE::Init).
		// Resolved here once and cached; ProcessEvent dereferences the cache.
		g_taskInterface = F4SE::GetTaskInterface().get();

		auto* readSource = RE::TESBookReadEvent::GetEventSource();
		if (!readSource) {
			a_log.Error("note probe: TESBookReadEvent source unavailable — note prototype disabled (no subscriptions registered)");
			return false;
		}
		auto* transferSource = RE::TESContainerChangedEvent::GetEventSource();
		if (!transferSource) {
			a_log.Error("note probe: TESContainerChangedEvent source unavailable — note prototype disabled (no subscriptions registered)");
			return false;
		}

		// Process-lifetime storage; F4SE plugins never unload, so there is no
		// teardown path by design (documented per the interop checklist).
		static NoteEventSink sink(a_log, a_verbose, a_sink);

		if (!readSource->RegisterSink(&sink)) {
			a_log.Error("note probe: TESBookReadEvent sink registration refused — note prototype disabled (no subscriptions registered)");
			return false;
		}
		if (!transferSource->RegisterSink(&sink)) {
			// Partial-failure symmetry: roll back the read subscription so no
			// half-probe keeps running.
			readSource->UnregisterSink(&sink);
			a_log.Error("note probe: TESContainerChangedEvent sink registration refused — read subscription rolled back, note prototype disabled");
			return false;
		}

		g_installed = true;
		a_log.Info(std::format("note probe: book-read + container-changed subscriptions installed (thread {} — game thread at kGameDataReady), verboseCapture={}",
			::GetCurrentThreadId(), a_verbose ? 1 : 0));
		return true;
	}
}
