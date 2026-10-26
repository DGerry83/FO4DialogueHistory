#pragma once

namespace F4DH::Application
{
	class IMessageSink;
}

namespace F4DH::Infrastructure
{
	// Observer on the engine's accepted-for-display HUD message value. A
	// REL::HookVft hook on HUDDataModel::PerformInputProcessing calls the
	// original first, then reads messagesModel.nextMessageData under its lock
	// and emits only new values. Empty/unchanged updates allocate nothing;
	// engine data is never mutated. Installed once at kGameDataReady for the
	// process lifetime, with no teardown.
	namespace HudMessageSink
	{
		// Returns false when the update observer cannot be installed — message
		// capture stays disabled and everything else keeps working. Verbose
		// diagnostics log the first and each 600th update plus value transitions.
		bool Install(Application::IMessageSink& sink, bool verboseDiagnostics);
	}
}
