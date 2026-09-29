#include "ScanCodeMap.h"

#include <charconv>

namespace
{
	constexpr std::size_t kMaxKeyNameLength{ 32 };

	struct KeyNameEntry
	{
		std::string_view name;
		std::uint32_t    scanCode;
	};

	// Name -> DirectInput scan code. All names are lowercase; the input is
	// lowercased before lookup. Single letters (a-z) are NOT listed here —
	// they resolve through the single-character path in ParseScanCode.
	constexpr KeyNameEntry kKeyNames[]{
		// row-independent named keys
		{ "escape", 1 }, { "esc", 1 },
		{ "backspace", 14 },
		{ "tab", 15 },
		{ "enter", 28 }, { "return", 28 },
		{ "capslock", 58 }, { "caps", 58 },
		{ "space", 57 }, { "spacebar", 57 },
		// modifiers (bare names map to the left variant)
		{ "lshift", 42 }, { "shift", 42 },
		{ "rshift", 54 },
		{ "lctrl", 29 }, { "lcontrol", 29 }, { "ctrl", 29 }, { "control", 29 },
		{ "rctrl", 157 }, { "rcontrol", 157 },
		{ "lalt", 56 }, { "alt", 56 },
		{ "ralt", 184 },
		// function keys
		{ "f1", 59 }, { "f2", 60 }, { "f3", 61 }, { "f4", 62 }, { "f5", 63 },
		{ "f6", 64 }, { "f7", 65 }, { "f8", 66 }, { "f9", 67 }, { "f10", 68 },
		{ "f11", 87 }, { "f12", 88 },
		// navigation cluster
		{ "home", 199 },
		{ "up", 200 }, { "uparrow", 200 }, { "arrowup", 200 },
		{ "pageup", 201 }, { "pgup", 201 },
		{ "left", 203 }, { "leftarrow", 203 }, { "arrowleft", 203 },
		{ "right", 205 }, { "rightarrow", 205 }, { "arrowright", 205 },
		{ "end", 207 },
		{ "down", 208 }, { "downarrow", 208 }, { "arrowdown", 208 },
		{ "pagedown", 209 }, { "pgdn", 209 },
		{ "insert", 210 }, { "ins", 210 },
		{ "delete", 211 }, { "del", 211 },
		// numpad
		{ "numlock", 69 },
		{ "numpad0", 82 }, { "numpad1", 79 }, { "numpad2", 80 }, { "numpad3", 81 },
		{ "numpad4", 75 }, { "numpad5", 76 }, { "numpad6", 77 },
		{ "numpad7", 71 }, { "numpad8", 72 }, { "numpad9", 73 },
		{ "numenter", 156 }, { "numpadenter", 156 },
		{ "numplus", 78 }, { "numadd", 78 }, { "numpadplus", 78 },
		{ "numminus", 74 }, { "numsubtract", 74 }, { "numpadminus", 74 },
		{ "nummultiply", 55 }, { "numstar", 55 },
		{ "numdivide", 181 }, { "numslash", 181 },
		{ "numdecimal", 83 }, { "numdot", 83 },
		// punctuation (name forms; several symbol characters double as INI
		// comment delimiters, so the spelled-out names are the reliable forms)
		{ "minus", 12 }, { "-", 12 },
		{ "equals", 13 }, { "=", 13 },
		{ "lbracket", 26 }, { "[", 26 },
		{ "rbracket", 27 }, { "]", 27 },
		{ "semicolon", 39 },
		{ "apostrophe", 40 }, { "'", 40 },
		{ "grave", 41 }, { "backtick", 41 }, { "`", 41 },
		{ "backslash", 43 }, { "\\", 43 },
		{ "comma", 51 }, { ",", 51 },
		{ "period", 52 }, { ".", 52 },
		{ "slash", 53 }, { "/", 53 },
		// disable aliases (meaningful for StressTestKey; rejected by Hotkey)
		{ "off", 0 }, { "none", 0 }, { "disabled", 0 },
	};

	// QWERTY letter rows in DirectInput order.
	[[nodiscard]] std::uint32_t LetterScanCode(char a_lower) noexcept
	{
		switch (a_lower) {
		case 'q': return 16;
		case 'w': return 17;
		case 'e': return 18;
		case 'r': return 19;
		case 't': return 20;
		case 'y': return 21;
		case 'u': return 22;
		case 'i': return 23;
		case 'o': return 24;
		case 'p': return 25;
		case 'a': return 30;
		case 's': return 31;
		case 'd': return 32;
		case 'f': return 33;
		case 'g': return 34;
		case 'h': return 35;
		case 'j': return 36;
		case 'k': return 37;
		case 'l': return 38;
		case 'z': return 44;
		case 'x': return 45;
		case 'c': return 46;
		case 'v': return 47;
		case 'b': return 48;
		case 'n': return 49;
		case 'm': return 50;
		default:  return 0;
		}
	}
}

namespace F4DH::Core
{
	std::optional<std::uint32_t> ParseScanCode(std::string_view a_value) noexcept
	{
		while (!a_value.empty() && (a_value.front() == ' ' || a_value.front() == '\t')) {
			a_value.remove_prefix(1);
		}
		while (!a_value.empty() && (a_value.back() == ' ' || a_value.back() == '\t')) {
			a_value.remove_suffix(1);
		}
		if (a_value.empty() || a_value.size() > kMaxKeyNameLength) {
			return std::nullopt;
		}

		// Lowercase into a fixed buffer (noexcept); reject anything outside
		// printable ASCII while copying.
		char lower[kMaxKeyNameLength]{};
		for (std::size_t i = 0; i < a_value.size(); ++i) {
			const auto c = static_cast<unsigned char>(a_value[i]);
			if (c < 0x21 || c > 0x7E) {
				return std::nullopt;
			}
			lower[i] = static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
		}
		const std::string_view name{ lower, a_value.size() };

		// All-digits values are raw scan codes (the original numeric schema) —
		// never key names, so "5" keeps meaning DIK 5.
		if (name.find_first_not_of("0123456789") == std::string_view::npos) {
			std::uint32_t code = 0;
			const auto* const first = name.data();
			const auto* const last  = first + name.size();
			const auto        result = std::from_chars(first, last, code, 10);
			if (result.ec == std::errc{} && result.ptr == last && code <= 0xFF) {
				return code;
			}
			return std::nullopt;
		}

		// Single letters resolve directly to their QWERTY scan code.
		if (name.size() == 1 && name.front() >= 'a' && name.front() <= 'z') {
			return LetterScanCode(name.front());
		}

		for (const auto& entry : kKeyNames) {
			if (entry.name == name) {
				return entry.scanCode;
			}
		}
		return std::nullopt;
	}
}
