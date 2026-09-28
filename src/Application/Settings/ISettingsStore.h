#pragma once

#include "Settings.h"

namespace F4DH::Application
{
	// Abstracts loading Settings from persistent storage.
	class ISettingsStore
	{
	public:
		virtual ~ISettingsStore() = default;

		// Reads settings, applying defaults for missing values and clamping ranges.
		[[nodiscard]] virtual Settings Load() = 0;

		// Persists panel geometry (M5). Called on the game thread when the
		// view reports a finished drag/resize; implementations may rewrite
		// their backing store, preserving unrelated settings.
		virtual void SaveGeometry(const Core::PanelGeometry& a_geometry) = 0;
	};
}
