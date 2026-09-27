#include "Composition.h"

// Milestone 1: construct the object graph here, owned for process lifetime:
//   Log -> Settings (IniSettingsStore) -> DialogueBuffer -> ViewController
//   (PrismaViewBridge or NullViewBridge) -> CapturePipeline -> SubtitleHook.
// Instances must be function-local statics or a leaked struct — F4SE plugins
// have no unload phase.

namespace F4DH
{
	bool Composition::InitializePostLoad()
	{
		return true;  // stub — milestone 1
	}

	bool Composition::InitializeDataLoaded()
	{
		return true;  // stub — milestone 1
	}
}
