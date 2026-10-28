#pragma once

#include "RE/B/BSTEvent.hpp"

namespace RE
{
	class TESBookReadEvent;
	class TESContainerChangedEvent;
}

namespace F4DH::Core
{
	class ILogger;
}

namespace F4DH::Infrastructure
{
	// M1 diagnostic prototype for Note History (v1.4.0): subscribes to the
	// engine's book-read and container-changed event sources and logs what the
	// events actually carry on OG/AE. ProcessEvent runs on whatever thread the
	// engine dispatches on — it performs NO engine calls and NO logging: it
	// copies bounded POD identity fields plus the originating thread ID and
	// marshals them to the game thread through the F4SE task interface, where
	// NoteRecordResolver performs all form/script/text/calendar/location
	// probes and writes the log lines. Logging-only: it holds no note state,
	// mutates nothing, and always returns kContinue so vanilla propagation
	// is unchanged. Diagnostics are gated on the existing VerboseCapture
	// setting; when off, ProcessEvent is a bare kContinue. Installed once at
	// kGameDataReady; successful subscriptions live for the process (F4SE
	// plugins never unload) and Install unregisters any partially registered
	// sink on failure.
	class NoteEventSink final
		: public RE::BSTEventSink<RE::TESBookReadEvent>
		, public RE::BSTEventSink<RE::TESContainerChangedEvent>
	{
	public:
		NoteEventSink(Core::ILogger& a_log, bool a_verbose);

		RE::BSEventNotifyControl ProcessEvent(const RE::TESBookReadEvent& a_event, RE::BSTEventSource<RE::TESBookReadEvent>*) override;
		RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent& a_event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override;

		// Returns false when either event source is unavailable or the second
		// registration fails after the first succeeded (the first is then
		// unregistered, leaving no partial subscription). Degraded, never
		// fatal — everything else keeps working without the note probe.
		[[nodiscard]] static bool Install(Core::ILogger& a_log, bool a_verbose);

	private:
		Core::ILogger& _log;
		bool           _verbose;
	};
}
