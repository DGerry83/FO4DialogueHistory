#include "IniSettingsStore.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "REX/Log.hpp"
#include "REX/W32/KERNEL32.hpp"

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
			REX::LogWarning(L"FO4DialogueHistory.ini not found ({}); using defaults", path);
			return settings;
		}

		bool        inSettingsSection{ false };
		std::string line;

		// M5 panel geometry — all four keys must be present and sane, else the
		// default centered layout stays. Coordinates are unsigned (the view
		// clamps the panel inside the viewport) with a generous upper bound.
		constexpr std::uint32_t kMaxPanelExtent{ 16384 };
		std::optional<std::uint32_t> panelX;
		std::optional<std::uint32_t> panelY;
		std::optional<std::uint32_t> panelWidth;
		std::optional<std::uint32_t> panelHeight;

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
					REX::LogWarning("IniSettingsStore: invalid Hotkey '{}' — keeping default 35", value);
				} else {
					settings.hotkeyScanCode = parsed;
				}
			} else if (key == "buffersize") {
				if (!ok) {
					REX::LogWarning("IniSettingsStore: invalid BufferSize '{}' — keeping default 0 (unlimited)", value);
				} else if (parsed == 0) {
					settings.bufferSize = 0;  // unlimited — no eviction
				} else if (parsed < 10 || parsed > 500) {
					settings.bufferSize = std::clamp(parsed, 10u, 500u);
					REX::LogWarning("IniSettingsStore: BufferSize {} out of range 10-500 — clamped to {}", parsed, settings.bufferSize);
				} else {
					settings.bufferSize = parsed;
				}
			} else if (key == "fontsize") {
				if (!ok) {
					REX::LogWarning("IniSettingsStore: invalid FontSize '{}' — keeping default 16", value);
				} else if (parsed < 10 || parsed > 32) {
					settings.fontSize = static_cast<int>(std::clamp(parsed, 10u, 32u));
					REX::LogWarning("IniSettingsStore: FontSize {} out of range 10-32 — clamped to {}", parsed, settings.fontSize);
				} else {
					settings.fontSize = static_cast<int>(parsed);
				}
			} else if (key == "panelx") {
				if (ok) {
					panelX = parsed;
				}
			} else if (key == "panely") {
				if (ok) {
					panelY = parsed;
				}
			} else if (key == "panelwidth") {
				if (ok) {
					panelWidth = parsed;
				}
			} else if (key == "panelheight") {
				if (ok) {
					panelHeight = parsed;
				}
			}
			// Unknown keys are ignored.
		}

		if (panelX && panelY && panelWidth && panelHeight &&
			*panelWidth >= Core::kPanelMinWidth && *panelWidth <= kMaxPanelExtent &&
			*panelHeight >= Core::kPanelMinHeight && *panelHeight <= kMaxPanelExtent &&
			*panelX <= kMaxPanelExtent && *panelY <= kMaxPanelExtent) {
			settings.panelGeometry = Core::PanelGeometry{
				static_cast<int>(*panelX),
				static_cast<int>(*panelY),
				static_cast<int>(*panelWidth),
				static_cast<int>(*panelHeight)
			};
		} else if (panelX || panelY || panelWidth || panelHeight) {
			REX::LogWarning("IniSettingsStore: incomplete or invalid panel geometry — using default centered layout");
		}

		return settings;
	}

	void IniSettingsStore::SaveGeometry(const Core::PanelGeometry& a_geometry)
	{
		const auto path = ResolveIniPath();

		// Rewrite the file, preserving every foreign line verbatim. The four
		// panel keys are kept in one block directly under the [Settings]
		// header; a missing file/section gets one created. Runs on the game
		// thread at mouseup cadence (user gesture) — bounded and infrequent.
		std::vector<std::string> lines;
		{
			std::ifstream in(path);
			std::string   line;
			while (std::getline(in, line)) {
				lines.push_back(line);  // keeps any existing \r; re-emitted verbatim
			}
		}

		const auto isPanelKey = [](std::string_view a_line) {
			std::string_view view{ a_line };
			if (!view.empty() && view.back() == '\r') {
				view.remove_suffix(1);
			}
			const auto eq = view.find('=');
			if (eq == std::string_view::npos) {
				return false;
			}
			const auto key = ToLower(Trim(view.substr(0, eq)));
			return key == "panelx" || key == "panely" || key == "panelwidth" || key == "panelheight";
		};

		const auto block = std::format("PanelX = {}\nPanelY = {}\nPanelWidth = {}\nPanelHeight = {}",
			a_geometry.x, a_geometry.y, a_geometry.width, a_geometry.height);

		std::vector<std::string> out;
		bool                     inserted{ false };
		for (const auto& raw : lines) {
			if (isPanelKey(raw)) {
				continue;  // stale value; the fresh block below is the only copy
			}
			out.push_back(raw);
			std::string_view view{ raw };
			if (!view.empty() && view.back() == '\r') {
				view.remove_suffix(1);
			}
			view = Trim(view);
			if (!inserted && view.size() >= 2 && view.front() == '[' && view.back() == ']' &&
				ToLower(Trim(view.substr(1, view.size() - 2))) == "settings") {
				out.push_back(block);
				inserted = true;
			}
		}
		if (!inserted) {
			if (!out.empty() && !out.back().empty()) {
				out.emplace_back();  // blank separator before the new section
			}
			out.emplace_back("[Settings]");
			out.push_back(block);
		}

		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		if (!file) {
			REX::LogError(L"IniSettingsStore: failed to open {} for writing — geometry not persisted", path);
			return;
		}
		for (const auto& outLine : out) {
			file << outLine << '\n';
		}
		REX::LogInformation("IniSettingsStore: panel geometry saved ({}x{} at {},{})",
			a_geometry.width, a_geometry.height, a_geometry.x, a_geometry.y);
	}
}
