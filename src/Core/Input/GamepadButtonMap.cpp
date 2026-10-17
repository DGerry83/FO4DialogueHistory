#include "GamepadButtonMap.h"

#include <charconv>

namespace
{
	// Sentinel for "no chord modifier". Must stay outside the 0-15 button
	// code space (0xFF is never a valid button).
	constexpr std::uint8_t kNoButton{ 0xFF };

	constexpr std::size_t kButtonCount{ 16 };

	// Canonical names indexed by device-local button code. This table is
	// the single source of truth for both parsing and GamepadButtonName.
	constexpr std::string_view kButtonNames[kButtonCount]{
		"DUp", "DDown", "DLeft", "DRight",
		"Start", "Back",
		"LS", "RS", "LB", "RB",
		"A", "B", "X", "Y",
		"LT", "RT",
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

	// Name -> device-local button code. Returns kNoButton for anything that
	// is not a canonical button name (including numerics — the numeric
	// schema is only accepted as a whole value by ParseGamepadHotkey).
	[[nodiscard]] std::uint8_t ParseButtonName(std::string_view a_name) noexcept
	{
		for (std::size_t i = 0; i < kButtonCount; ++i) {
			if (EqualsIgnoreCase(kButtonNames[i], a_name)) {
				return static_cast<std::uint8_t>(i);
			}
		}
		return kNoButton;
	}
}

namespace F4DH::Core
{
	GamepadHotkeyBinding ParseGamepadHotkey(std::string_view a_value) noexcept
	{
		constexpr GamepadHotkeyBinding kInvalid{ 0, kNoButton, false };

		a_value = Trim(a_value);
		if (a_value.empty()) {
			return kInvalid;
		}

		const auto plus = a_value.find('+');
		if (plus == std::string_view::npos) {
			// Single-button path: a bare decimal code 0-15 (the original
			// numeric schema) or a canonical name.
			if (a_value.find_first_not_of("0123456789") == std::string_view::npos) {
				int code = 0;
				const auto* const first = a_value.data();
				const auto* const last = first + a_value.size();
				const auto result = std::from_chars(first, last, code, 10);
				if (result.ec != std::errc{} || result.ptr != last || code > 15) {
					return kInvalid;
				}
				return { static_cast<std::uint8_t>(code), kNoButton, true };
			}
			const auto trigger = ParseButtonName(a_value);
			if (trigger == kNoButton) {
				return kInvalid;
			}
			return { trigger, kNoButton, true };
		}

		// Chord path: exactly one '+', both parts named buttons, and the
		// two parts must name distinct buttons (LB+LB is not a chord).
		if (a_value.find('+', plus + 1) != std::string_view::npos) {
			return kInvalid;
		}
		const auto modifier = ParseButtonName(Trim(a_value.substr(0, plus)));
		const auto trigger = ParseButtonName(Trim(a_value.substr(plus + 1)));
		if (modifier == kNoButton || trigger == kNoButton || modifier == trigger) {
			return kInvalid;
		}
		return { trigger, modifier, true };
	}

	std::string_view GamepadButtonName(std::uint8_t a_code) noexcept
	{
		if (a_code < kButtonCount) {
			return kButtonNames[a_code];
		}
		return "?";
	}
}
