#include "IniSettingsStore.h"

#include "Core/Input/ScanCodeMap.h"

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
// no file I/O anywhere else. Hand-rolled: seven keys, no dependency.
//
// M9: panel geometry lives in a SIBLING file (FO4DialogueHistory.geometry.ini)
// that release packages never ship, so redeploying the mod folder can no
// longer clobber a saved layout (the M5 in-place INI rewrite was wiped every
// whole-folder redeploy). Legacy Panel keys in the main INI are still honored
// as a fallback; SaveGeometry never touches the main INI anymore.

namespace
{
	constexpr std::wstring_view kIniFileName{ L"FO4DialogueHistory.ini" };
	constexpr std::wstring_view kGeometryFileName{ L"FO4DialogueHistory.geometry.ini" };

	[[nodiscard]] std::wstring ResolveSiblingPath(std::wstring_view a_fileName)
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
		path.append(a_fileName.data(), a_fileName.size());
		return path;
	}

	[[nodiscard]] std::vector<std::string> ReadLines(const std::wstring& a_path)
	{
		std::vector<std::string> lines;
		std::ifstream            in(a_path);
		std::string              line;
		while (std::getline(in, line)) {
			lines.push_back(line);
		}
		return lines;
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

	// Section-locked [Settings] scan for the panel quad. All four keys must be
	// present and sane (M5 rules), else nullopt: coordinates unsigned with a
	// generous upper bound; the view clamps the panel inside the viewport.
	[[nodiscard]] std::optional<F4DH::Core::PanelGeometry> ExtractPanelGeometry(const std::vector<std::string>& a_lines)
	{
		constexpr std::uint32_t kMaxPanelExtent{ 16384 };
		std::optional<std::uint32_t> panelX;
		std::optional<std::uint32_t> panelY;
		std::optional<std::uint32_t> panelWidth;
		std::optional<std::uint32_t> panelHeight;
		bool                         inSettingsSection{ false };

		for (const auto& line : a_lines) {
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
			if (!inSettingsSection) {
				continue;
			}

			const auto eq = view.find('=');
			if (eq == std::string_view::npos) {
				continue;
			}
			const auto key   = ToLower(Trim(view.substr(0, eq)));
			const auto value = Trim(view.substr(eq + 1));

			std::uint32_t parsed = 0;
			if (!ParseUint(value, parsed)) {
				continue;
			}
			if (key == "panelx") {
				panelX = parsed;
			} else if (key == "panely") {
				panelY = parsed;
			} else if (key == "panelwidth") {
				panelWidth = parsed;
			} else if (key == "panelheight") {
				panelHeight = parsed;
			}
		}

		if (panelX && panelY && panelWidth && panelHeight &&
			*panelWidth >= F4DH::Core::kPanelMinWidth && *panelWidth <= kMaxPanelExtent &&
			*panelHeight >= F4DH::Core::kPanelMinHeight && *panelHeight <= kMaxPanelExtent &&
			*panelX <= kMaxPanelExtent && *panelY <= kMaxPanelExtent) {
			return F4DH::Core::PanelGeometry{
				static_cast<int>(*panelX),
				static_cast<int>(*panelY),
				static_cast<int>(*panelWidth),
				static_cast<int>(*panelHeight)
			};
		}
		return std::nullopt;
	}
}

namespace F4DH::Infrastructure
{
	Application::Settings IniSettingsStore::Load()
	{
		Application::Settings settings;

		const auto path = ResolveSiblingPath(kIniFileName);
		const auto lines = ReadLines(path);
		if (lines.empty()) {
			REX::LogWarning(L"FO4DialogueHistory.ini not found ({}); using defaults", path);
		}

		// Section tracking: [Settings] holds the capture/display schema,
		// [Diagnostics] the logging switches and the stress-test keys. Unknown
		// sections are ignored.
		std::string currentSection;

		for (const auto& line : lines) {
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
				currentSection = ToLower(Trim(view.substr(1, view.size() - 2)));
				continue;
			}

			const auto eq = view.find('=');
			if (eq == std::string_view::npos) {
				continue;  // not a key/value line
			}

			const auto key   = ToLower(Trim(view.substr(0, eq)));
			const auto value = Trim(view.substr(eq + 1));

			if (currentSection == "diagnostics") {
				if (key == "verbosecapture") {
					std::uint32_t parsed = 0;
					if (!ParseUint(value, parsed)) {
						REX::LogWarning("IniSettingsStore: invalid VerboseCapture '{}' — keeping default 0 (off)", value);
					} else if (parsed > 1) {
						settings.verboseCapture = true;
						REX::LogWarning("IniSettingsStore: VerboseCapture {} out of range 0-1 — clamped to 1", parsed);
					} else {
						settings.verboseCapture = parsed == 1;
					}
				} else if (key == "keyprobe") {
					std::uint32_t parsed = 0;
					if (!ParseUint(value, parsed)) {
						REX::LogWarning("IniSettingsStore: invalid KeyProbe '{}' — keeping default 0 (off)", value);
					} else if (parsed > 1) {
						settings.keyProbe = true;
						REX::LogWarning("IniSettingsStore: KeyProbe {} out of range 0-1 — clamped to 1", parsed);
					} else {
						settings.keyProbe = parsed == 1;
					}
				} else if (key == "stresstestkey") {
					if (const auto code = Core::ParseScanCode(value)) {
						settings.stressTestKey = *code;  // 0 = disabled (numeric 0 or off/none/disabled)
					} else {
						REX::LogWarning("IniSettingsStore: invalid StressTestKey '{}' — keeping default 0 (disabled)", value);
					}
				} else if (key == "stresstestlines") {
					std::uint32_t parsed = 0;
					if (!ParseUint(value, parsed)) {
						REX::LogWarning("IniSettingsStore: invalid StressTestLines '{}' — keeping default 500", value);
					} else if (parsed < 1 || parsed > 5000) {
						settings.stressTestLines = std::clamp(parsed, 1u, 5000u);
						REX::LogWarning("IniSettingsStore: StressTestLines {} out of range 1-5000 — clamped to {}", parsed, settings.stressTestLines);
					} else {
						settings.stressTestLines = parsed;
					}
				} else if (key == "stresstestquests") {
					std::uint32_t parsed = 0;
					if (!ParseUint(value, parsed)) {
						REX::LogWarning("IniSettingsStore: invalid StressTestQuests '{}' — keeping default 20", value);
					} else if (parsed < 1 || parsed > 100) {
						settings.stressTestQuests = std::clamp(parsed, 1u, 100u);
						REX::LogWarning("IniSettingsStore: StressTestQuests {} out of range 1-100 — clamped to {}", parsed, settings.stressTestQuests);
					} else {
						settings.stressTestQuests = parsed;
					}
				}
				continue;  // unknown [Diagnostics] keys ignored
			}

			if (currentSection != "settings") {
				continue;  // locked schema reads [Settings] and [Diagnostics] only
			}

			std::uint32_t parsed = 0;
			const bool    ok = ParseUint(value, parsed);

			if (key == "hotkey") {
				if (const auto code = Core::ParseScanCode(value)) {
					if (*code == 0) {
						REX::LogWarning("IniSettingsStore: Hotkey '{}' would disable the panel toggle — keeping default 35 (H)", value);
					} else {
						settings.hotkeyScanCode = *code;
					}
				} else {
					REX::LogWarning("IniSettingsStore: invalid Hotkey '{}' — keeping default 35 (H)", value);
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
			}
			// Panel keys are handled by ExtractPanelGeometry; unknown keys ignored.
		}

		// Geometry precedence (M9): the sibling geometry file wins; legacy
		// Panel keys in the main INI are the fallback; else default layout.
		settings.panelGeometry = ExtractPanelGeometry(lines);
		const auto geometryPath = ResolveSiblingPath(kGeometryFileName);
		const auto geometryLines = ReadLines(geometryPath);
		if (!geometryLines.empty()) {
			if (const auto fromFile = ExtractPanelGeometry(geometryLines)) {
				settings.panelGeometry = *fromFile;
			} else {
				REX::LogWarning("IniSettingsStore: geometry file present but invalid — ignoring it");
			}
		}
		if (settings.panelGeometry) {
			const auto& g = *settings.panelGeometry;
			REX::LogInformation("IniSettingsStore: panel geometry loaded ({}x{} at {},{})", g.width, g.height, g.x, g.y);
		}

		return settings;
	}

	void IniSettingsStore::SaveGeometry(const Core::PanelGeometry& a_geometry)
	{
		// M9: write ONLY the sibling geometry file — never the user's main
		// INI. The file is not part of the release package, so whole-folder
		// redeploys cannot clobber it. Runs on the game thread at mouseup
		// cadence (user gesture) — bounded and infrequent.
		const auto path = ResolveSiblingPath(kGeometryFileName);

		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		if (!file) {
			REX::LogError(L"IniSettingsStore: failed to open {} for writing — geometry not persisted", path);
			return;
		}
		file << "[Settings]\n"
			<< std::format("PanelX = {}\nPanelY = {}\nPanelWidth = {}\nPanelHeight = {}\n",
				a_geometry.x, a_geometry.y, a_geometry.width, a_geometry.height);
		REX::LogInformation("IniSettingsStore: panel geometry saved ({}x{} at {},{})",
			a_geometry.width, a_geometry.height, a_geometry.x, a_geometry.y);
	}
}
