#include "Composition.h"

#include <format>

#include <Windows.h>  // MapVirtualKeyW — OG keyMask carries VK codes (C11)

#include "Application/Capture/CapturePipeline.h"
#include "Application/Input/HotkeyController.h"
#include "Application/Settings/Settings.h"
#include "Application/StressTest/StressTestInjector.h"
#include "Application/View/ViewController.h"
#include "Core/Dialogue/DialogueBuffer.h"
#include "Core/Input/GamepadButtonMap.h"
#include "Infrastructure/Cosave/CosaveStore.h"
#include "Infrastructure/Hooks/SubtitleHook.h"
#include "Infrastructure/Input/GameplayInputGate.h"
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

		// "No chord modifier" sentinel from Core::GamepadButtonMap (0 — no
		// gamepad button is coded 0 in the XInput-mask code space).
		constexpr std::uint16_t kNoGamepadModifier{ Core::kNoGamepadButton };

		// The engine delivers Windows virtual-key codes in ButtonEvent.keyMask
		// on every desktop runtime — OG confirmed by the C10 diagnostic dumps,
		// AE 1.11.240 confirmed 2026-10-18 (H arrives as 72/0x48, Esc as 27).
		// Translate the configured DIK scan code to its VK equivalent once at
		// init. The INI setting stays documented as a DirectInput scan code.
		[[nodiscard]] MatchPair ResolveHotkeyMatchCode(std::uint32_t a_scanCode, Core::ILogger& a_log)
		{
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

			a_log.Info(std::format("hotkey: DIK {} -> VK {}", a_scanCode, vk));
			return { vk, 0 };
		}

		// Edge-triggered keyboard router for the panel hotkey. Inserted at the
		// FRONT of MenuControls::handlers (#005: typing-scoped kStop consumption
		// must run ahead of vanilla handlers and later plugins) for the process
		// lifetime (never removed; F4SE plugins never unload).
		class InputSink final : public RE::BSInputEventUser
		{
		public:
			InputSink(Application::HotkeyController& hotkey, Core::ILogger& log, MatchPair a_match, std::uint32_t a_configuredScanCode,
				MatchPair a_stressMatch = { 0, 0 }, std::uint32_t a_stressScanCode = 0, bool a_probe = false,
				Core::GamepadHotkeyBinding a_gpBinding = {}, bool a_exclusiveTyping = true) :
				_hotkey(hotkey),
				_log(log),
				_match(a_match),
				_configuredScanCode(a_configuredScanCode),
				_stressMatch(a_stressMatch),
				_stressScanCode(a_stressScanCode),
				_probe(a_probe),
				_gpBinding(a_gpBinding),
				_exclusiveTyping(a_exclusiveTyping)
			{}

			// The dispatcher consults this before delivering; the base
			// implementation returns false, which left this sink opted out of
			// every event (C9 hotkey fix). Accept only what OnButtonEvent acts
			// on — keyboard button events, plus gamepad button events while
			// the KeyProbe diagnostic is on (C1) or a gamepad hotkey binding
			// is configured (C4).
			bool ShouldHandleEvent(const RE::InputEvent* a_event) override
			{
				const bool accept = a_event &&
					((a_event->Is(RE::INPUT_EVENT_TYPE::kButton) &&
						 a_event->device == RE::INPUT_DEVICE::kKeyboard) ||
						((_probe || _gpBinding.enabled) && a_event->device == RE::INPUT_DEVICE::kGamepad &&
							a_event->Is(RE::INPUT_EVENT_TYPE::kButton)));
				if (accept && a_event->device == RE::INPUT_DEVICE::kKeyboard && !_loggedFirstEvent) {
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

				// C1: KeyProbe gamepad probe — log one line per delivered
				// gamepad button event (idCode = XInput bit mask, LT/RT on
				// F4SE's 0x09/0x0A; decoded name appended), analog value,
				// hold time, press state. Never consumed, never matched
				// against the keyboard hotkey/stress pairs.
				if (a_event->device == RE::INPUT_DEVICE::kGamepad) {
					if (_probe) {
						const auto idCode = static_cast<std::uint32_t>(a_event->QIDCode());
						const char* state = a_event->QJustPressed() ? "pressed" :
							(a_event->QReleased() ? "released" : "held");
						_log.Info(std::format("input probe: gamepad button idCode {} (0x{:X}) [{}] value {:.2f} held {:.2f}s state {}",
							idCode, idCode, Core::GamepadButtonName(static_cast<std::uint16_t>(idCode & 0xFFFF)),
							a_event->value, a_event->heldDownSecs, state));
					}

					// C4: gamepad hotkey routing — only when a binding is
					// configured (ShouldHandleEvent already gated delivery).
					// Edge-triggered like the keyboard path: the chord
					// modifier is tracked with one POD bool from its
					// press/release edges (release clears held even if the
					// trigger never fired); the trigger fires on JustPressed
					// only, and only while the modifier is held when a chord
					// is configured. Never sets `handled` — vanilla bindings
					// (VATS, favorites, ...) keep working.
					//
					// #007 (2026-10-19): the chord is OPEN-ONLY. While the panel
					// is open the view owns the controller (B / Start close) and
					// PrismaUI's V12 action capture swallows action-bound buttons
					// (LB/RB/X/Y/LT/RT/Start) above the MenuControls dispatch —
					// runtime-observed: LB's release edge never arrived once the
					// panel took focus, the held flag stuck true, and D-pad
					// navigation then fired the chord. Matching is suspended
					// while open and the held flag resets on every fire, so a
					// swallowed edge can never invert the state.
					if (_gpBinding.enabled) {
						const auto idCode = static_cast<std::uint32_t>(a_event->QIDCode());
						if (_gpBinding.modifier != kNoGamepadModifier && idCode == _gpBinding.modifier) {
							_gpModifierHeld = a_event->QJustPressed() ? true :
								(a_event->QReleased() ? false : _gpModifierHeld);
							return;
						}
						if (idCode == _gpBinding.trigger && a_event->QJustPressed() &&
							!_hotkey.IsPanelOpen() &&
							(_gpBinding.modifier == kNoGamepadModifier || _gpModifierHeld)) {
							_gpModifierHeld = false;  // #007: a swallowed modifier release must not outlive the open
							if (_gpBinding.modifier == kNoGamepadModifier) {
								_log.Info(std::format("input: gamepad hotkey matched ({}), opening panel",
									Core::GamepadButtonName(_gpBinding.trigger)));
							} else {
								_log.Info(std::format("input: gamepad hotkey matched ({}+{}), opening panel",
									Core::GamepadButtonName(_gpBinding.modifier), Core::GamepadButtonName(_gpBinding.trigger)));
							}
							_hotkey.OnGamepadButton();
						}
					}
					return;
				}

				const auto code = static_cast<std::uint32_t>(a_event->QIDCode());

				if (a_event->device != RE::INPUT_DEVICE::kKeyboard) {
					return;
				}

				// #005: while a view text input owns typing, every keyboard
				// ButtonEvent is captive — our own hotkey/stress routing is
				// suppressed (always; typing an 'H' must not close the panel),
				// and with [Settings] ExclusiveTypingInput=1 the event is
				// consumed (handled=kStop) for every handler after us — press
				// AND release alike, so no stuck-key states. `handled` is a
				// writable engine field (PrismaUI sets kStop from its own
				// MenuControls sink for gamepad events); the sink signature is
				// const, hence the cast. When NOT typing this branch never runs
				// and behavior is byte-identical to before.
				if (_hotkey.IsTypingActive()) {
					if (_exclusiveTyping) {
						const_cast<RE::ButtonEvent*>(a_event)->handled = RE::InputEvent::HANDLED_RESULT::kStop;
					}
					if (_probe) {
						_log.Info(std::format("input probe: key {} code {} (0x{:X}) [suppressed: typing]",
							a_event->QJustPressed() ? "press" : (a_event->QReleased() ? "release" : "hold"),
							code, code));
					}
					return;
				}

				if (!a_event->QJustPressed()) {
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
			Core::GamepadHotkeyBinding     _gpBinding{};         // C4: parsed [Settings] GamepadHotkey; enabled=false = unconfigured
			bool                           _exclusiveTyping{ true };  // #005: [Settings] ExclusiveTypingInput — gates ONLY the kStop consumption, never the own-hotkey suppression
			bool                           _gpModifierHeld{ false };  // C4: chord modifier held, tracked from press/release edges
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

		// #005 C2: gameplay-input gate — the C1 MenuControls swallow cannot
		// reach PlayerControls user-event consumers, so WASD/camera leaked
		// while typing. The BSInputEnableManager layer is allocated once here
		// (game thread, data ready); allocation failure degrades to a no-op
		// gate, never fatal. ExclusiveTypingInput=0 leaves the gate inert.
		static Infrastructure::GameplayInputGate   gameplayGate(root.log, root.settings.exclusiveTypingInput);
		gameplayGate.Initialize();

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

		// C4: [Settings] GamepadHotkey (parsed by Core::ParseGamepadHotkey at
		// INI load, C3). The sink matches the binding edge-for-edge; the
		// controller just toggles. Disabled binding = zero-initialized POD,
		// sink gamepad branch stays inert.
		const auto& gpBinding = root.settings.gamepadHotkey;
		if (gpBinding.enabled) {
			if (gpBinding.modifier == kNoGamepadModifier) {
				root.log.Info(std::format("input: gamepad hotkey bound: {}",
					Core::GamepadButtonName(gpBinding.trigger)));
			} else {
				root.log.Info(std::format("input: gamepad hotkey bound: {}+{}",
					Core::GamepadButtonName(gpBinding.modifier), Core::GamepadButtonName(gpBinding.trigger)));
			}
			// RB (0x0200) and RT (0x000A) are never delivered as ButtonEvents on
			// the MenuControls chain (runtime-observed 2026-10-19, OG 1.10.163 —
			// their in-game actions fire, so the engine reads them outside the
			// event stream). A binding using either can never fire — warn loudly.
			constexpr std::uint16_t kUndeliveredRB{ 0x0200 }, kUndeliveredRT{ 0x000A };
			if (gpBinding.trigger == kUndeliveredRB || gpBinding.trigger == kUndeliveredRT ||
				gpBinding.modifier == kUndeliveredRB || gpBinding.modifier == kUndeliveredRT) {
				root.log.Warn("input: gamepad hotkey uses RB/RT — the engine never delivers those as button events, the binding will NOT fire; pick another button");
			}
		}
		static InputSink                           inputSink(hotkey, root.log, matchCode, root.settings.hotkeyScanCode, stressMatchCode, stressScanCode, root.settings.keyProbe, gpBinding, root.settings.exclusiveTypingInput);

		bridge.SetFontSize(root.settings.fontSize);
		bridge.SetGeometry(root.settings.panelGeometry);
		bridge.SetFilters(root.settings.viewFilters);
		bridge.SetSettingsSink(&settingsStore);
		// #005: the view's typingStateChanged reports (dispatched to the game
		// thread by the bridge) land on the ViewController flag the sink reads.
		bridge.SetTypingStateCallback([](bool typing) { viewController.SetTyping(typing); });
		viewController.SetGameplayInputGate(&gameplayGate);
		Application::ViewController::SetCloseTarget(&viewController);

		if (const auto menuControls = RE::MenuControls::GetSingleton()) {
			// The AV fork exposes no RegisterHandler engine wrapper; the
			// handlers array is the registration point. Inserted at the FRONT
			// (#005: the typing-scoped kStop consumption must run ahead of
			// vanilla handlers and later plugins) for the process lifetime
			// (no removal — F4SE plugins never unload).
			menuControls->handlers.insert(menuControls->handlers.begin(), &inputSink);
			root.log.Info("hotkey input sink registered (front of handler chain)");
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
