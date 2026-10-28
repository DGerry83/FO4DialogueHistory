#pragma once

#include "Application/Settings/ISettingsStore.h"

namespace F4DH::Infrastructure
{
	// Reads Data/F4SE/Plugins/FO4DialogueHistory.ini. Missing file or invalid
	// values fall back to defaults (clamped to the ranges in the design spec).
	// M9: panel geometry lives in a sibling FO4DialogueHistory.geometry.ini
	// that release packages never ship (redeploy-proof); legacy Panel keys in
	// the main INI are honored as a fallback. The main INI is never written.
	// The quest-bucket filter checkboxes persist the same way in a sibling
	// FO4DialogueHistory.view.ini ([Settings] FilterMain/Side/Misc/Unattributed
	// as 0/1; absent file or keys = all-true defaults). View preferences
	// (font size + PauseOnOpen) persist in a sibling
	// FO4DialogueHistory.preferences.ini ([Settings] FontSize (10–48) and
	// PauseOnOpen (0/1); numeric FontSize outside the range clamps to the
	// nearest boundary) whose keys override the main-INI seed independently.
	// Message History exclusions load separately from the user-editable sibling
	// FO4DialogueHistory.MessageFilters.ini ([MessageFilters] ExcludedPrefix).
	class IniSettingsStore final : public Application::ISettingsStore
	{
	public:
		[[nodiscard]] Application::Settings Load() override;
		void SaveGeometry(const Core::PanelGeometry& a_geometry) override;
		void SaveViewFilters(const Core::ViewFilterState& a_filters) override;
		void SaveViewPreferences(const Core::ViewPreferences& a_preferences) override;
	};
}
