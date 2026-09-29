#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Geometry/PanelGeometry.h"

namespace F4DH::Application
{
	// User-facing settings. Hotkey/buffer/font load once at plugin load and
	// are immutable afterwards. Panel geometry is absent until the user moves
	// or resizes the panel (default: centered layout); it is read from the INI
	// at load and rewritten when a drag ends (M5).
	struct Settings
	{
		std::uint32_t hotkeyScanCode = 35;  // [Settings] Hotkey as a DIK scan code (INI accepts key names; Core::ParseScanCode resolves them). Default DIK_H
		std::size_t   bufferSize = 0;       // 0 = unlimited (no eviction); >0 caps, range 10–500
		int           fontSize = 16;        // range 10–32 (px)
		bool          verboseCapture = false;  // [Diagnostics] VerboseCapture: log every subtitle event at Information level
		std::uint32_t stressTestKey = 0;       // [Diagnostics] StressTestKey: DIK scan code (INI accepts key names + "off") injecting one synthetic batch; 0 = disabled
		std::uint32_t stressTestLines = 500;   // [Diagnostics] StressTestLines: lines per injected batch (clamped 1-5000)
		std::uint32_t stressTestQuests = 20;   // [Diagnostics] StressTestQuests: synthetic quests per batch (clamped 1-100)
		std::optional<Core::PanelGeometry> panelGeometry;  // absent = default centered layout
	};
}
