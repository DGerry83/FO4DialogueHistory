#pragma once

namespace F4DH::Application
{
	class ISubtitleSink;
}

namespace F4DH::Infrastructure
{
	// Detours SubtitleManager::ShowSubtitle (Address Library REL::ID 2249542)
	// and translates engine arguments into Core::SubtitleEvent.
	// Installed once at F4SE kPostLoad; process-lifetime, no teardown.
	namespace SubtitleHook
	{
		// Returns false if the trampoline could not be installed (fail fast).
		bool Install(Application::ISubtitleSink& sink);
	}
}
