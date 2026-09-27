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
	};
}
