#include "IniSettingsStore.h"

// Milestone 5: parse "Data/F4SE/Plugins/FO4DialogueHistory.ini"
// ([Settings] Hotkey / BufferSize / FontSize), clamp BufferSize to 10–500
// and FontSize to 10–32, log a warning on missing/invalid values.

namespace F4DH::Infrastructure
{
	Application::Settings IniSettingsStore::Load()
	{
		return {};  // stub: defaults — milestone 5
	}
}
