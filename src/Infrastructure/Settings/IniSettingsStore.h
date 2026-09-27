#pragma once

#include "Application/Settings/ISettingsStore.h"

namespace F4DH::Infrastructure
{
	// Reads Data/F4SE/Plugins/FO4DialogueHistory.ini. Missing file or invalid
	// values fall back to defaults (clamped to the ranges in the design spec).
	class IniSettingsStore final : public Application::ISettingsStore
	{
	public:
		[[nodiscard]] Application::Settings Load() override;
	};
}
