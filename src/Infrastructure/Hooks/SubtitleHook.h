#pragma once

namespace F4DH::Application
{
	class ISubtitleSink;
}

namespace F4DH::Infrastructure
{
	// Detours SubtitleManager::ShowSubtitle (Address Library variant ID:
	// OG 875508 / NG 2249542 — see kShowSubtitle in the .cpp)
	// and translates engine arguments into Core::SubtitleEvent.
	// Installed once at F4SE kPostLoad; process-lifetime, no teardown.
	namespace SubtitleHook
	{
		// Returns false if the trampoline could not be installed (fail fast).
		bool Install(Application::ISubtitleSink& sink);
	}
}
