#include "IniSettingsStore.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <string>
#include <string_view>

#include "REX/LOG.h"
#include "REX/W32/KERNEL32.h"

// Reads FO4DialogueHistory.ini from the folder that hosts this DLL
// (Data/F4SE/Plugins under the game root — resolved via the module path so
// non-standard Data locations still work). One-shot parse at kGameDataReady;
// no file I/O anywhere else. Hand-rolled: three keys, no dependency.

namespace
{
	constexpr std::wstring_view kIniFileName{ L"FO4DialogueHistory.ini" };

	[[nodiscard]] std::wstring ResolveIniPath()
	{
		wchar_t buffer[1024]{};
		const auto length = REX::W32::GetModuleFileNameW(REX::W32::GetCurrentModule(), buffer, std::size(buffer));
		std::wstring path{ buffer, length };
		const auto   sep = path.find_last_of(L"\\/");
		if (sep == std::wstring::npos) {
			path.clear();
		} else {
			path.resize(sep + 1);
		}
		path.append(kIniFileName.data(), kIniFileName.size());
		return path;
	}

	[[nodiscard]] std::string_view Trim(std::string_view a_value)
	{
		while (!a_value.empty() && (a_value.front() == ' ' || a_value.front() == '\t')) {
			a_value.remove_prefix(1);
		}
		while (!a_value.empty() && (a_value.back() == ' ' || a_value.back() == '\t')) {
			a_value.remove_suffix(1);
		}
		return a_value;
	}

	[[nodiscard]] std::string ToLower(std::string_view a_value)
	{
		std::string out{ a_value };
		for (auto& c : out) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return out;
	}

	// Strict unsigned parse: the whole trimmed value must be consumed.
	[[nodiscard]] bool ParseUint(std::string_view a_text, std::uint32_t& a_out)
	{
		if (a_text.empty()) {
			return false;
		}
		const auto* const first = a_text.data();
		const auto* const last  = first + a_text.size();
		const auto        result = std::from_chars(first, last, a_out, 10);
		return result.ec == std::errc{} && result.ptr == last;
	}
}

namespace F4DH::Infrastructure
{
	Application::Settings IniSettingsStore::Load()
	{
		Application::Settings settings;

		const auto path = ResolveIniPath();
		std::ifstream in(path);
		if (!in) {
			REX::WARN(L"FO4DialogueHistory.ini not found ({}); using defaults", path);
			return settings;
		}

		bool        inSettingsSection{ false };
		std::string line;
		while (std::getline(in, line)) {
			std::string_view view{ line };

			const auto comment = view.find_first_of(";#");
			if (comment != std::string_view::npos) {
				view = view.substr(0, comment);
			}
			view = Trim(view);
			if (view.empty()) {
				continue;
			}

			if (view.front() == '[' && view.back() == ']') {
				inSettingsSection = ToLower(Trim(view.substr(1, view.size() - 2))) == "settings";
				continue;
			}

			const auto eq = view.find('=');
			if (eq == std::string_view::npos) {
				continue;  // not a key/value line
			}
			if (!inSettingsSection) {
				continue;  // locked schema reads [Settings] only
			}

			const auto key   = ToLower(Trim(view.substr(0, eq)));
			const auto value = Trim(view.substr(eq + 1));

			std::uint32_t parsed = 0;
			const bool    ok = ParseUint(value, parsed);

			if (key == "hotkey") {
				if (!ok) {
					REX::WARN("IniSettingsStore: invalid Hotkey '{}' — keeping default 35", value);
				} else {
					settings.hotkeyScanCode = parsed;
				}
			} else if (key == "buffersize") {
				if (!ok) {
					REX::WARN("IniSettingsStore: invalid BufferSize '{}' — keeping default 50", value);
				} else if (parsed < 10 || parsed > 500) {
					settings.bufferSize = std::clamp(parsed, 10u, 500u);
					REX::WARN("IniSettingsStore: BufferSize {} out of range 10-500 — clamped to {}", parsed, settings.bufferSize);
				} else {
					settings.bufferSize = parsed;
				}
			} else if (key == "fontsize") {
				if (!ok) {
					REX::WARN("IniSettingsStore: invalid FontSize '{}' — keeping default 16", value);
				} else if (parsed < 10 || parsed > 32) {
					settings.fontSize = static_cast<int>(std::clamp(parsed, 10u, 32u));
					REX::WARN("IniSettingsStore: FontSize {} out of range 10-32 — clamped to {}", parsed, settings.fontSize);
				} else {
					settings.fontSize = static_cast<int>(parsed);
				}
			}
			// Unknown keys are ignored.
		}

		return settings;
	}
}
