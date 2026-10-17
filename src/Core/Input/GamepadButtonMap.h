#pragma once

#include <cstdint>
#include <string_view>

namespace F4DH::Core
{
	// Parsed form of the [Settings] GamepadHotkey INI value: which gamepad
	// button (or chord) toggles the dialogue-history view.
	//
	// `trigger` and `modifier` hold device-local button codes 0-15:
	//   DUp=0, DDown=1, DLeft=2, DRight=3, Start=4, Back=5,
	//   LS=6, RS=7, LB=8, RB=9, A=10, B=11, X=12, Y=13, LT=14, RT=15
	//
	// `modifier` is the chord's first button. When the binding has no
	// chord it holds the sentinel 0xFF — NOT 0, because code 0 is DUp, a
	// valid button. `enabled` is false when the INI value was empty or
	// invalid; consumers must check it before acting on the codes.
	struct GamepadHotkeyBinding
	{
		std::uint8_t trigger;
		std::uint8_t modifier;
		bool         enabled;
	};

	// Parses a GamepadHotkey INI value into a GamepadHotkeyBinding.
	//
	// Accepted forms (case-insensitive, surrounding whitespace ignored):
	//   - button names: "DUp", "DDown", "DLeft", "DRight", "Start", "Back",
	//     "LS", "RS", "LB", "RB", "A", "B", "X", "Y", "LT", "RT"
	//   - chords "Mod+Trigger" — exactly one '+', both parts must be named
	//     buttons, distinct from each other ("LB+DUp"); whitespace around
	//     each part is tolerated
	//   - a bare decimal button code 0-15 (the original numeric schema,
	//     mirroring Hotkey accepting a raw DIK code)
	//
	// Anything else — empty, unknown names, bad or out-of-range numbers,
	// duplicate chord buttons, more than two parts, numbers inside a chord —
	// yields { 0, 0xFF, false }. Never throws; the caller keeps the
	// feature off.
	[[nodiscard]] GamepadHotkeyBinding ParseGamepadHotkey(std::string_view a_value) noexcept;

	// Canonical name for a device-local button code (0-15), for logging.
	// Returns "?" for out-of-range codes.
	[[nodiscard]] std::string_view GamepadButtonName(std::uint8_t a_code) noexcept;
}
