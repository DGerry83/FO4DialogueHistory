#pragma once

#include <cstdint>
#include <string_view>

namespace F4DH::Core
{
	// "No chord modifier" sentinel. 0 is safe: the engine's gamepad code
	// space (below) contains no button coded 0.
	inline constexpr std::uint16_t kNoGamepadButton{ 0 };

	// Parsed form of the [Settings] GamepadHotkey INI value: which gamepad
	// button (or chord) toggles the dialogue-history view.
	//
	// `trigger` and `modifier` hold the codes the engine delivers in gamepad
	// ButtonEvent.idCode — XInput bit masks for the digital buttons, F4SE's
	// special codes for the analog triggers (runtime-confirmed 2026-10-19 on
	// OG 1.10.163 via KeyProbe):
	//   DUp=0x0001, DDown=0x0002, DLeft=0x0004, DRight=0x0008,
	//   Start=0x0010, Back=0x0020, LS=0x0040, RS=0x0080,
	//   LB=0x0100, RB=0x0200,
	//   A=0x1000, B=0x2000, X=0x4000, Y=0x8000,
	//   LT=0x0009, RT=0x000A (F4SE::Inputs::XInput::kGameButton_*Trigger)
	//
	// `modifier` is the chord's first button. When the binding has no chord
	// it holds the sentinel kNoGamepadButton. `enabled` is false when the
	// INI value was empty or invalid; consumers must check it before acting
	// on the codes.
	struct GamepadHotkeyBinding
	{
		std::uint16_t trigger;
		std::uint16_t modifier;
		bool          enabled;
	};

	// Parses a GamepadHotkey INI value into a GamepadHotkeyBinding.
	//
	// Accepted forms (case-insensitive, surrounding whitespace ignored):
	//   - button names: "DUp", "DDown", "DLeft", "DRight", "Start", "Back",
	//     "LS", "RS", "LB", "RB", "A", "B", "X", "Y", "LT", "RT"
	//   - chords "Mod+Trigger" — exactly one '+', both parts must be named
	//     buttons, distinct from each other ("LB+DUp"); whitespace around
	//     each part is tolerated
	//   - a bare decimal button code, accepted only when it exactly equals
	//     one of the 16 known codes above ("256" = LB, "9" = LT); any other
	//     number is invalid
	//
	// Anything else — empty, unknown names, unknown numbers, duplicate chord
	// buttons, more than two parts, numbers inside a chord — yields
	// { kNoGamepadButton, kNoGamepadButton, false }. Never throws; the caller
	// keeps the feature off.
	[[nodiscard]] GamepadHotkeyBinding ParseGamepadHotkey(std::string_view a_value) noexcept;

	// Canonical name for a gamepad button code, for logging.
	// Returns "?" for codes outside the known set.
	[[nodiscard]] std::string_view GamepadButtonName(std::uint16_t a_code) noexcept;
}
