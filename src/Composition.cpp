#include "Composition.h"

#include "Application/Capture/CapturePipeline.h"
#include "Application/Settings/Settings.h"
#include "Application/View/ViewController.h"
#include "Core/Dialogue/DialogueBuffer.h"
#include "Infrastructure/Hooks/SubtitleHook.h"
#include "Infrastructure/Logging/Log.h"
#include "Infrastructure/Prisma/PrismaViewBridge.h"

// Process-lifetime object graph, held by function-local statics (F4SE
// plugins never unload). M1: log + settings + buffer. C3: view bridge (stub
// until C4), view controller, capture pipeline, subtitle hook. C5 adds the
// INI-backed settings store.

namespace F4DH
{
	namespace
	{
		struct CompositionRoot
		{
			Infrastructure::Log   log;
			Application::Settings settings;                    // shipped defaults; INI arrives in C5
			Core::DialogueBuffer  buffer{ settings.bufferSize };
			bool                  hookInstalled{ false };
		};

		CompositionRoot& Root()
		{
			static CompositionRoot root;
			return root;
		}
	}

	bool Composition::InitializePostLoad()
	{
		auto& root = Root();

		// C4 replaces the stub bridge with real PrismaUI calls; the pipeline
		// and hook are live either way, so capture logging works from M3 on.
		static Infrastructure::PrismaViewBridge bridge;
		static Application::ViewController      viewController(root.buffer, bridge);
		static Application::CapturePipeline     pipeline(root.buffer, viewController, root.log);

		if (root.hookInstalled) {
			root.log.Info("post-load initialization already complete");
			return true;
		}

		if (!Infrastructure::SubtitleHook::Install(pipeline)) {
			root.log.Error("failed to install subtitle hook; plugin inert");
			return false;
		}

		root.hookInstalled = true;
		root.log.Info("post-load initialization complete (subtitle hook installed)");
		return true;
	}

	bool Composition::InitializeDataLoaded()
	{
		Root().log.Info("data-loaded initialization complete");
		return true;
	}
}
