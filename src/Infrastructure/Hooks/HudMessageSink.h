#pragma once

namespace F4DH::Application
{
	class IMessageSink;
}

namespace F4DH::Infrastructure
{
	// Observer on the engine's accepted-for-display HUD message value. A
	// continuation-safe GameUIModel::UpdateDataModels hook calls the original
	// first, then reads HUDDataModel::messagesModel.nextMessageData under its
	// lock and emits only new values. Empty/unchanged updates allocate nothing;
	// engine data is never mutated. Installed once at kGameDataReady for the
	// process lifetime, with no teardown.
	namespace HudMessageSink
	{
		// Returns false when the display-data event source cannot be located
		// or created — message capture stays disabled, dialogue capture and
		// everything else keep working (degraded, never fatal).
		bool Install(Application::IMessageSink& sink);
	}
}
