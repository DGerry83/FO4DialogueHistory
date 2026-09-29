#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace F4DH::Core
{
	// Parses an INI key-binding value into a DirectInput (DIK) scan code.
	//
	// Accepted forms (case-insensitive, surrounding whitespace ignored):
	//   - key names: single letters ("H"), "F1"-"F12", "Space", "Enter",
	//     "Tab", "Escape", "Backspace", "CapsLock", "LShift"/"RShift",
	//     "LCtrl"/"RCtrl", "LAlt"/"RAlt", arrows ("Up"/"Down"/"Left"/"Right"),
	//     "Home"/"End"/"PageUp"/"PageDown"/"Insert"/"Delete",
	//     "Numpad0"-"Numpad9", "NumPlus"/"NumMinus"/"NumMultiply"/
	//     "NumDivide"/"NumDecimal"/"NumEnter"/"NumLock", symbol names
	//     ("Minus", "Equals", "Comma", "Period", "Slash", "Backslash",
	//     "Semicolon", "Apostrophe", "Grave", "LBracket", "RBracket")
	//   - disable aliases "off"/"none"/"disabled" -> 0
	//   - a plain decimal scan code (0-255) — the original numeric schema.
	//     A value that is all digits is ALWAYS a raw scan code, never a key
	//     name ("5" means DIK 5, not the 5 key).
	//
	// Returns nullopt for anything unrecognized, blank, over-long, or
	// containing non-printable characters — the caller keeps its default.
	[[nodiscard]] std::optional<std::uint32_t> ParseScanCode(std::string_view a_value) noexcept;
}
