#pragma once

namespace F4DH::Application
{
	class IMessageSink;
}

namespace F4DH::Infrastructure
{
	// Passive observer on the engine's accepted-for-display HUD message
	// channel: the global BSTGlobalEvent::EventSource<HUDMessageDisplayDataEvent>
	// (the value the HUDMessagesModel broadcasts when it supplies the HUD's
	// next display payload — throttled/suppressed submissions never reach it;
	// see the session ARCHITECTURE_CONTRACT). The sink only ever observes:
	// it returns kContinue, never consumes or mutates the event, performs no
	// I/O or form lookups, and copies the resolved text into owned storage.
	// Installed once at kGameDataReady; process-lifetime, no teardown.
	namespace HudMessageSink
	{
		// Returns false when the display-data event source cannot be located
		// or created — message capture stays disabled, dialogue capture and
		// everything else keep working (degraded, never fatal).
		bool Install(Application::IMessageSink& sink);
	}
}
