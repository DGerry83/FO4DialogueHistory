#pragma once

#include "Application/Settings/ISettingsStore.h"

namespace F4DH::Infrastructure
{
	// Reads Data/F4SE/Plugins/FO4DialogueHistory.ini. Missing file or invalid
	// values fall back to defaults (clamped to the ranges in the design spec).
	// M9: panel geometry lives in a sibling FO4DialogueHistory.geometry.ini
	// that release packages never ship (redeploy-proof); legacy Panel keys in
	// the main INI are honored as a fallback. The main INI is never written.
	class IniSettingsStore final : public Application::ISettingsStore
	{
	public:
		[[nodiscard]] Application::Settings Load() override;
		void SaveGeometry(const Core::PanelGeometry& a_geometry) override;
	};
}
