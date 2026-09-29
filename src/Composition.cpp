#include "Composition.h"

#include <format>

#include <Windows.h>  // MapVirtualKeyW — OG keyMask carries VK codes (C11)

#include "Application/Capture/CapturePipeline.h"
#include "Application/Input/HotkeyController.h"
#include "Application/Settings/Settings.h"
#include "Application/StressTest/StressTestInjector.h"
#include "Application/View/ViewController.h"
#include "Core/Dialogue/DialogueBuffer.h"
#include "Infrastructure/Cosave/CosaveStore.h"
#include "Infrastructure/Hooks/SubtitleHook.h"
#include "Infrastructure/Logging/Log.h"
#include "Infrastructure/Prisma/PrismaViewBridge.h"
#include "Infrastructure/Settings/IniSettingsStore.h"

#include "RE/B/BSInputEventUser.hpp"
#include "RE/B/ButtonEvent.hpp"
#include "RE/M/MenuControls.hpp"

// Process-lifetime object graph, held by function-local statics (F4SE
// plugins never unload). C5: the INI is parsed once at kGameDataReady and
// the whole graph — buffer sized from it, view bridge + controller +
// pipeline, hotkey + input sink, subtitle hook — builds immediately after,
// in InitializeDataLoaded. (FallHook installs the same ShowSubtitle prologue
// hook even earlier, at F4SEPlugin_Load; no subtitle can fire before
// kGameDataReady.)

namespace F4DH
{
	namespace
	{
		struct CompositionRoot
		{
			Infrastructure::Log   log;
			Application::Settings settings;  // defaults until the INI load at kGameDataReady
			bool                  graphReady{ false };
		};

		CompositionRoot& Root()
		{
			static CompositionRoot root;
			return root;
		}

		// DirectInput extended scan codes: MapVirtualKeyW only maps them when
		// prefixed with 0xE000 (extended-key flag in the high word).
		[[nodiscard]] constexpr bool IsExtendedDik(std::uint32_t a_scanCode)
		{
			switch (a_scanCode) {
			case 0x9C:  // DIK_NUMPADENTER
			case 0x9D:  // DIK_RCONTROL
			case 0xB5:  // DIK_DIVIDE
			case 0xB7:  // DIK_RWIN
			case 0xB8:  // DIK_RMENU
			case 0xC7:  // DIK_HOME
			case 0xC8:  // DIK_UP
			case 0xC9:  // DIK_PRIOR
			case 0xCB:  // DIK_LEFT
			case 0xCD:  // DIK_RIGHT
			case 0xCF:  // DIK_END
			case 0xD0:  // DIK_DOWN
			case 0xD1:  // DIK_NEXT
			case 0xD2:  // DIK_INSERT
			case 0xD3:  // DIK_DELETE
				return true;
			default:
				return false;
			}
		}

		// Numpad digit/decimal scan codes are NumLock-sensitive: Windows reports
		// VK_NUMPADx with NumLock on and the navigation key that shares the scan
		// code (Insert/End/Down/...) with it off. MapVirtualKeyW only ever
		// returns the navigation VK for these (0x52 -> VK_INSERT), so both
		// equivalents must be matched. Pair: { NumLock-on VK, NumLock-off VK }.
		[[nodiscard]] constexpr std::pair<std::uint32_t, std::uint32_t> NumpadVkPair(std::uint32_t a_scanCode)
		{
			switch (a_scanCode) {
			case 0x52: return { 0x60, 0x2D };  // DIK_NUMPAD0  -> VK_NUMPAD0 / VK_INSERT
			case 0x4F: return { 0x61, 0x23 };  // DIK_NUMPAD1  -> VK_NUMPAD1 / VK_END
			case 0x50: return { 0x62, 0x28 };  // DIK_NUMPAD2  -> VK_NUMPAD2 / VK_DOWN
			case 0x51: return { 0x63, 0x22 };  // DIK_NUMPAD3  -> VK_NUMPAD3 / VK_NEXT
			case 0x4B: return { 0x64, 0x25 };  // DIK_NUMPAD4  -> VK_NUMPAD4 / VK_LEFT
			case 0x4C: return { 0x65, 0x0C };  // DIK_NUMPAD5  -> VK_NUMPAD5 / VK_CLEAR
			case 0x4D: return { 0x66, 0x27 };  // DIK_NUMPAD6  -> VK_NUMPAD6 / VK_RIGHT
			case 0x47: return { 0x67, 0x24 };  // DIK_NUMPAD7  -> VK_NUMPAD7 / VK_HOME
			case 0x48: return { 0x68, 0x26 };  // DIK_NUMPAD8  -> VK_NUMPAD8 / VK_UP
			case 0x49: return { 0x69, 0x21 };  // DIK_NUMPAD9  -> VK_NUMPAD9 / VK_PRIOR
			case 0x53: return { 0x6E, 0x2E };  // DIK_DECIMAL  -> VK_DECIMAL / VK_DELETE
			default:   return { 0, 0 };
			}
		}

		// Two engine codes that should trigger one binding (0 = no secondary).
		struct MatchPair
		{
			std::uint32_t primary;
			std::uint32_t secondary;
		};

		// C11: OG 1.10.163 delivers Windows virtual-key codes in
		// ButtonEvent.keyMask (empirically confirmed by the C10 diagnostic
		// dumps — H arrived as 72/0x48); NG/AE deliver DirectInput scan codes.
		// Translate the configured DIK scan code to its VK equivalent once, on
		// OG only. The INI setting stays documented as a DirectInput scan code.
		[[nodiscard]] MatchPair ResolveHotkeyMatchCode(std::uint32_t a_scanCode, Core::ILogger& a_log)
		{
			if (F4SE::GetRuntimeType() != F4SE::RuntimeType::kOG) {
				a_log.Info(std::format("hotkey: DIK {} kept as raw scan code (NG/AE runtime)", a_scanCode));
				return { a_scanCode, 0 };
			}

			if (const auto numpad = NumpadVkPair(a_scanCode); numpad.first != 0) {
				a_log.Info(std::format("hotkey: DIK {} -> VK {} / VK {} (numpad key, both NumLock states matched)", a_scanCode, numpad.first, numpad.second));
				return { numpad.first, numpad.second };
			}

			const auto mappedInput = static_cast<UINT>(IsExtendedDik(a_scanCode) ? (a_scanCode | 0xE000u) : a_scanCode);
			const auto vk = static_cast<std::uint32_t>(::MapVirtualKeyW(mappedInput, MAPVK_VSC_TO_VK));
			if (vk == 0) {
				a_log.Warn(std::format("hotkey: DIK {} has no VK mapping; comparing raw scan code", a_scanCode));
				return { a_scanCode, 0 };
			}

			a_log.Info(std::format("hotkey: DIK {} -> VK {} (OG runtime)", a_scanCode, vk));
			return { vk, 0 };
		}

		// Edge-triggered keyboard router for the panel hotkey. Appended to
		// MenuControls::handlers for the process lifetime (never removed;
		// F4SE plugins never unload).
		class InputSink final : public RE::BSInputEventUser
		{
		public:
			InputSink(Application::HotkeyController& hotkey, Core::ILogger& log, MatchPair a_match, std::uint32_t a_configuredScanCode,
				MatchPair a_stressMatch = { 0, 0 }, std::uint32_t a_stressScanCode = 0, bool a_probe = false) :
				_hotkey(hotkey),
				_log(log),
				_match(a_match),
				_configuredScanCode(a_configuredScanCode),
				_stressMatch(a_stressMatch),
				_stressScanCode(a_stressScanCode),
				_probe(a_probe)
			{}

			// The dispatcher consults this before delivering; the base
			// implementation returns false, which left this sink opted out of
			// every event (C9 hotkey fix). Accept only what OnButtonEvent acts
			// on — keyboard button events.
			bool ShouldHandleEvent(const RE::InputEvent* a_event) override
			{
				const bool accept = a_event &&
					a_event->Is(RE::INPUT_EVENT_TYPE::kButton) &&
					a_event->device == RE::INPUT_DEVICE::kKeyboard;
				if (accept && !_loggedFirstEvent) {
					_loggedFirstEvent = true;
					_log.Info("input: first keyboard button event received");
				}
				return accept;
			}

			void OnButtonEvent(const RE::ButtonEvent* a_event) override
			{
				if (!a_event) {
					return;
				}

				const auto code = static_cast<std::uint32_t>(a_event->QIDCode());

				if (a_event->device != RE::INPUT_DEVICE::kKeyboard || !a_event->QJustPressed()) {
					return;
				}
				// C11: on OG the engine code is a VK (translated at init into
				// _match); on NG/AE it is the raw DIK scan code. Only the
				// configured DIK code is ever handed down, so the Application
				// layer never sees VK values. The secondary covers the numpad
				// cluster's second NumLock state. The stress-test pair follows
				// the same rule; a 0 stress match code disables the branch
				// entirely.
				const bool hotkeyHit  = code == _match.primary || (_match.secondary != 0 && code == _match.secondary);
				const bool stressHit  = !hotkeyHit && _stressMatch.primary != 0 &&
					(code == _stressMatch.primary || (_stressMatch.secondary != 0 && code == _stressMatch.secondary));

				// [Diagnostics] KeyProbe: dump every delivered keyboard press so
				// bind problems can be mapped empirically in ONE game session.
				// If a physical key produces no line here, the engine never
				// delivered it to MenuControls handlers — no bind can catch it.
				if (_probe) {
					_log.Info(std::format("input probe: key press code {} (0x{:X}){}",
						code, code,
						hotkeyHit ? " [panel hotkey]" : (stressHit ? " [stress-test key]" : "")));
				}

				if (hotkeyHit) {
					_log.Info(std::format("input: hotkey matched (code {}), toggling panel", code));
					_hotkey.OnKeyDown(_configuredScanCode);
				} else if (stressHit) {
					_log.Info(std::format("input: stress-test key matched (code {}), injecting batch", code));
					_hotkey.OnKeyDown(_stressScanCode);
				}
			}

		private:
			Application::HotkeyController& _hotkey;
			Core::ILogger&                 _log;
			MatchPair                      _match;               // DIK on NG/AE, VK on OG; secondary = numpad NumLock twin
			std::uint32_t                  _configuredScanCode;  // always DIK (INI value)
			MatchPair                      _stressMatch;         // DIK on NG/AE, VK on OG; primary 0 = disabled
			std::uint32_t                  _stressScanCode;      // always DIK (INI value)
			bool                           _probe{ false };      // [Diagnostics] KeyProbe
			bool                           _loggedFirstEvent{ false };
		};
	}

	bool Composition::InitializePostLoad()
	{
		Root().log.Info("post-load phase reached; object graph builds at game-data-ready (INI load)");
		return true;
	}

	bool Composition::InitializeDataLoaded()
	{
		auto& root = Root();

		if (root.graphReady) {
			root.log.Info("data-loaded initialization already complete");
			return true;
		}

		static Infrastructure::IniSettingsStore settingsStore;
		root.settings = settingsStore.Load();
		root.log.Info(std::format(
			"settings: hotkey={} bufferSize={} fontSize={} verboseCapture={} keyProbe={}",
			root.settings.hotkeyScanCode,
			root.settings.bufferSize,
			root.settings.fontSize,
			root.settings.verboseCapture,
			root.settings.keyProbe));

		static Core::DialogueBuffer                buffer{ root.settings.bufferSize };
		static Infrastructure::PrismaViewBridge    bridge;
		static Application::ViewController         viewController(buffer, bridge, root.log);
		static Application::CapturePipeline        pipeline(buffer, viewController, root.log, root.settings.verboseCapture);
		static Application::StressTestInjector     stressInjector(buffer, viewController, root.log, root.settings);
		static Application::HotkeyController       hotkey(root.settings, viewController, stressInjector);

		// The stress key gets its own match pair, resolved through the same
		// DIK->VK translation as the panel hotkey and only when configured:
		// the default 0 (or a duplicate of the panel hotkey) leaves the
		// InputSink branch disabled — the panel toggle always wins the conflict.
		std::uint32_t stressScanCode = root.settings.stressTestKey;
		if (stressScanCode != 0 && stressScanCode == root.settings.hotkeyScanCode) {
			root.log.Warn(std::format("settings: StressTestKey {} duplicates the panel hotkey — stress-test key disabled (panel toggle wins)", stressScanCode));
			stressScanCode = 0;
		}
		const auto                                 matchCode = ResolveHotkeyMatchCode(root.settings.hotkeyScanCode, root.log);
		const auto                                 stressMatchCode = stressScanCode != 0 ? ResolveHotkeyMatchCode(stressScanCode, root.log) : MatchPair{ 0, 0 };
		static InputSink                           inputSink(hotkey, root.log, matchCode, root.settings.hotkeyScanCode, stressMatchCode, stressScanCode, root.settings.keyProbe);

		bridge.SetFontSize(root.settings.fontSize);
		bridge.SetGeometry(root.settings.panelGeometry);
		bridge.SetSettingsSink(&settingsStore);
		Application::ViewController::SetCloseTarget(&viewController);

		if (const auto menuControls = RE::MenuControls::GetSingleton()) {
			// The AV fork exposes no RegisterHandler engine wrapper; the
			// handlers array is the registration point. Appended for the
			// process lifetime (no removal — F4SE plugins never unload).
			menuControls->handlers.push_back(&inputSink);
			root.log.Info("hotkey input sink registered");
		} else {
			root.log.Error("MenuControls unavailable — panel hotkey disabled");
		}

		if (!Infrastructure::SubtitleHook::Install(pipeline)) {
			root.log.Error("failed to install subtitle hook; plugin inert");
			return false;
		}

		// M3: hand the buffer to the cosave store (registered at plugin load).
		// The serialization callbacks run on the game thread, same as capture.
		Infrastructure::CosaveStore::Bind(&buffer);

		root.graphReady = true;
		root.log.Info("data-loaded initialization complete (object graph built, subtitle hook installed)");
		return true;
	}
}
