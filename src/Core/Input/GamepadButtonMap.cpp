#include "GamepadButtonMap.h"

#include <charconv>

namespace
{
	struct ButtonEntry
	{
		std::string_view name;
		std::uint16_t    code;
	};

	constexpr std::size_t kButtonCount{ 16 };

	// Canonical name -> engine idCode (XInput bit mask; LT/RT are F4SE's
	// special codes 0x09/0x0A). This table is the single source of truth
	// for both parsing and GamepadButtonName.
	constexpr ButtonEntry kButtons[kButtonCount]{
		{ "DUp", 0x0001 }, { "DDown", 0x0002 }, { "DLeft", 0x0004 }, { "DRight", 0x0008 },
		{ "Start", 0x0010 }, { "Back", 0x0020 },
		{ "LS", 0x0040 }, { "RS", 0x0080 }, { "LB", 0x0100 }, { "RB", 0x0200 },
		{ "A", 0x1000 }, { "B", 0x2000 }, { "X", 0x4000 }, { "Y", 0x8000 },
		{ "LT", 0x0009 }, { "RT", 0x000A },
	};

	[[nodiscard]] std::string_view Trim(std::string_view a_value) noexcept
	{
		while (!a_value.empty() && (a_value.front() == ' ' || a_value.front() == '\t')) {
			a_value.remove_prefix(1);
		}
		while (!a_value.empty() && (a_value.back() == ' ' || a_value.back() == '\t')) {
			a_value.remove_suffix(1);
		}
		return a_value;
	}

	// ASCII case-insensitive equality; anything outside A-Z/a-z must match
	// exactly, so junk characters can never alias a button name.
	[[nodiscard]] bool EqualsIgnoreCase(std::string_view a_lhs, std::string_view a_rhs) noexcept
	{
		if (a_lhs.size() != a_rhs.size()) {
			return false;
		}
		for (std::size_t i = 0; i < a_lhs.size(); ++i) {
			const auto l = static_cast<unsigned char>(a_lhs[i]);
			const auto r = static_cast<unsigned char>(a_rhs[i]);
			const auto ll = l >= 'A' && l <= 'Z' ? l + ('a' - 'A') : l;
			const auto rr = r >= 'A' && r <= 'Z' ? r + ('a' - 'A') : r;
			if (ll != rr) {
				return false;
			}
		}
		return true;
	}

	// Name -> engine button code. Returns kNoGamepadButton for anything that
	// is not a canonical button name (including numerics — the numeric
	// schema is only accepted as a whole value by ParseGamepadHotkey).
	[[nodiscard]] std::uint16_t ParseButtonName(std::string_view a_name) noexcept
	{
		for (const auto& button : kButtons) {
			if (EqualsIgnoreCase(button.name, a_name)) {
				return button.code;
			}
		}
		return F4DH::Core::kNoGamepadButton;
	}

	// Code -> canonical name; kNoGamepadButton when the code is unknown.
	[[nodiscard]] std::uint16_t KnownButtonCode(std::uint32_t a_code) noexcept
	{
		for (const auto& button : kButtons) {
			if (button.code == a_code) {
				return button.code;
			}
		}
		return F4DH::Core::kNoGamepadButton;
	}
}

namespace F4DH::Core
{
	GamepadHotkeyBinding ParseGamepadHotkey(std::string_view a_value) noexcept
	{
		constexpr GamepadHotkeyBinding kInvalid{ kNoGamepadButton, kNoGamepadButton, false };

		a_value = Trim(a_value);
		if (a_value.empty()) {
			return kInvalid;
		}

		const auto plus = a_value.find('+');
		if (plus == std::string_view::npos) {
			// Single-button path: a bare decimal code, accepted only when it
			// exactly equals one of the 16 known masks, or a canonical name.
			if (a_value.find_first_not_of("0123456789") == std::string_view::npos) {
				std::uint32_t code = 0;
				const auto* const first = a_value.data();
				const auto* const last = first + a_value.size();
				const auto result = std::from_chars(first, last, code, 10);
				if (result.ec != std::errc{} || result.ptr != last) {
					return kInvalid;
				}
				const auto known = KnownButtonCode(code);
				if (known == kNoGamepadButton) {
					return kInvalid;
				}
				return { known, kNoGamepadButton, true };
			}
			const auto trigger = ParseButtonName(a_value);
			if (trigger == kNoGamepadButton) {
				return kInvalid;
			}
			return { trigger, kNoGamepadButton, true };
		}

		// Chord path: exactly one '+', both parts named buttons, and the
		// two parts must name distinct buttons (LB+LB is not a chord).
		if (a_value.find('+', plus + 1) != std::string_view::npos) {
			return kInvalid;
		}
		const auto modifier = ParseButtonName(Trim(a_value.substr(0, plus)));
		const auto trigger = ParseButtonName(Trim(a_value.substr(plus + 1)));
		if (modifier == kNoGamepadButton || trigger == kNoGamepadButton || modifier == trigger) {
			return kInvalid;
		}
		return { trigger, modifier, true };
	}

	std::string_view GamepadButtonName(std::uint16_t a_code) noexcept
	{
		for (const auto& button : kButtons) {
			if (button.code == a_code) {
				return button.name;
			}
		}
		return "?";
	}
}
