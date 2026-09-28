#pragma once

#include "Application/Settings/ISettingsStore.h"

namespace F4DH::Infrastructure
{
	// Reads Data/F4SE/Plugins/FO4DialogueHistory.ini. Missing file or invalid
	// values fall back to defaults (clamped to the ranges in the design spec).
	// M5 adds the minimal write path: panel geometry is rewritten in place,
	// preserving every other line of the file.
	class IniSettingsStore final : public Application::ISettingsStore
	{
	public:
		[[nodiscard]] Application::Settings Load() override;
		void SaveGeometry(const Core::PanelGeometry& a_geometry) override;
	};
}
