#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Geometry/PanelGeometry.h"
#include "Core/Input/GamepadButtonMap.h"
#include "Core/Message/MessageBuffer.h"
#include "Core/Settings/ViewFilterState.h"
#include "Core/Settings/ViewPreferences.h"

namespace F4DH::Application
{
	// User-facing settings. Hotkey and buffer load once at plugin load; view
	// preferences can also change through the panel. Panel geometry is absent
	// until the user moves or resizes the panel (default: centered layout); it
	// is read from the INI at load and rewritten when a drag ends (M5).
	struct Settings
	{
		std::uint32_t hotkeyScanCode = 35;  // [Settings] Hotkey as a DIK scan code (INI accepts key names; Core::ParseScanCode resolves them). Default DIK_H
		Core::GamepadHotkeyBinding gamepadHotkey{};  // [Settings] GamepadHotkey: gamepad button/chord that toggles the view (INI accepts names + chords; Core::ParseGamepadHotkey parses). Empty/absent = disabled
		bool          exclusiveTypingInput = true;  // [Settings] ExclusiveTypingInput: while a panel text input is focused, consume keyboard ButtonEvents (handled=kStop) AND block gameplay user events (movement/camera via BSInputEnableManager); 0 disables both — the panel hotkey stays suppressed while typing either way
		std::size_t   bufferSize = 0;       // 0 = unlimited (no eviction); >0 caps, range 10–500. Dialogue-only; message retention is MessageBufferSize
		std::size_t   messageBufferSize = Core::MessageBuffer::kDefaultCapacity;  // [Settings] MessageBufferSize: retained HUD messages, 1–1000 (0 clamps to 1); invalid keeps 200. Bounded FIFO — no unlimited mode
		bool          verboseCapture = false;  // [Diagnostics] VerboseCapture: log every subtitle event at Information level
		bool          keyProbe = false;        // [Diagnostics] KeyProbe: log every keyboard press code the engine delivers (bind diagnostics)
		std::uint32_t stressTestKey = 0;       // [Diagnostics] StressTestKey: DIK scan code (INI accepts key names + "off") injecting one synthetic batch; 0 = disabled
		std::uint32_t stressTestLines = 500;   // [Diagnostics] StressTestLines: lines per injected batch (clamped 1-5000)
		std::uint32_t stressTestQuests = 20;   // [Diagnostics] StressTestQuests: synthetic quests per batch (clamped 1-100)
		std::optional<Core::PanelGeometry> panelGeometry;  // absent = default centered layout
		Core::ViewFilterState viewFilters;  // quest-bucket checkboxes; default all-true, loaded from the sibling view.ini when present
		Core::ViewPreferences viewPreferences{};  // panel-wide font size + pause-on-open; the main INI seeds (FontSize 10–48, PauseOnOpen 0/1) and the sibling preferences.ini overrides per key (FontSize 10–48, numeric out-of-range clamps)
	};
}
