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
		std::uint32_t hotkeyScanCode = 35;  // provisional: DIK_H — verified against vanilla binds in milestone 4 (AC9)
		std::size_t   bufferSize = 0;       // 0 = unlimited (no eviction); >0 caps, range 10–500
		int           fontSize = 16;        // range 10–32 (px)
		std::optional<Core::PanelGeometry> panelGeometry;  // absent = default centered layout
	};
}
