#include "Composition.h"

#include "Application/Settings/Settings.h"
#include "Core/Dialogue/DialogueBuffer.h"
#include "Infrastructure/Logging/Log.h"

// M1 object graph: process-lifetime components only, held by a
// function-local static (F4SE plugins never unload). Wiring added in later
// chunks: SubtitleHook (C3), view bridge + ViewController (C4), INI-backed
// settings (C5).

namespace F4DH
{
	namespace
	{
		struct CompositionRoot
		{
			Infrastructure::Log   log;
			Application::Settings settings;                    // shipped defaults; INI arrives in C5
			Core::DialogueBuffer  buffer{ settings.bufferSize };
		};

		CompositionRoot& Root()
		{
			static CompositionRoot root;
			return root;
		}
	}

	bool Composition::InitializePostLoad()
	{
		Root().log.Info("post-load initialization complete");
		return true;
	}

	bool Composition::InitializeDataLoaded()
	{
		Root().log.Info("data-loaded initialization complete");
		return true;
	}
}
