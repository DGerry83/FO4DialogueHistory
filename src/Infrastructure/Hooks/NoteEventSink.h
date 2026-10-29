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

namespace F4DH::Application
{
	class INoteSink;
}

namespace F4DH::Infrastructure
{
	// Native book-read/container event delivery for Note History. Delivery
	// copies bounded identity fields and marshals them through F4SE tasks;
	// resolution and optional diagnostics run later. The application sink
	// receives pickup observations independently of VerboseCapture; READ is
	// diagnostic-only on OG, where PREPARED owns visible admission. Every
	// event returns kContinue and subscriptions roll back on partial failure.
	class NoteEventSink final
		: public RE::BSTEventSink<RE::TESBookReadEvent>
		, public RE::BSTEventSink<RE::TESContainerChangedEvent>
	{
	public:
		NoteEventSink(Core::ILogger& a_log, bool a_verbose, Application::INoteSink* a_sink = nullptr);

		RE::BSEventNotifyControl ProcessEvent(const RE::TESBookReadEvent& a_event, RE::BSTEventSource<RE::TESBookReadEvent>*) override;
		RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent& a_event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override;

		// Returns false when either event source is unavailable or the second
		// registration fails after the first succeeded (the first is then
		// unregistered, leaving no partial subscription). Degraded, never
		// fatal — everything else keeps working without the note probe.
		[[nodiscard]] static bool Install(Core::ILogger& a_log, bool a_verbose, Application::INoteSink* a_sink = nullptr);

	private:
		Core::ILogger& _log;
		bool           _verbose;
		Application::INoteSink* _sink;
	};
}
