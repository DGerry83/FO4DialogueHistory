#include "Composition.h"

#include <array>
#include <format>

#include <Windows.h>  // MapVirtualKeyW — OG keyMask carries VK codes (C11)

#include "Application/Capture/CapturePipeline.h"
#include "Application/Input/HotkeyController.h"
#include "Application/Settings/Settings.h"
#include "Application/View/ViewController.h"
#include "Core/Dialogue/DialogueBuffer.h"
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

		// C11: OG 1.10.163 delivers Windows virtual-key codes in
		// ButtonEvent.keyMask (empirically confirmed by the C10 diagnostic
		// dumps — H arrived as 72/0x48); NG/AE deliver DirectInput scan codes.
		// Translate the configured DIK scan code to its VK equivalent once, on
		// OG only. The INI setting stays documented as a DirectInput scan code.
		[[nodiscard]] std::uint32_t ResolveHotkeyMatchCode(std::uint32_t a_scanCode, Core::ILogger& a_log)
		{
			if (F4SE::GetRuntimeType() != F4SE::RuntimeType::kOG) {
				a_log.Info(std::format("hotkey: DIK {} kept as raw scan code (NG/AE runtime)", a_scanCode));
				return a_scanCode;
			}

			const auto mappedInput = static_cast<UINT>(IsExtendedDik(a_scanCode) ? (a_scanCode | 0xE000u) : a_scanCode);
			const auto vk = static_cast<std::uint32_t>(::MapVirtualKeyW(mappedInput, MAPVK_VSC_TO_VK));
			if (vk == 0) {
				a_log.Warn(std::format("hotkey: DIK {} has no VK mapping; comparing raw scan code", a_scanCode));
				return a_scanCode;
			}

			a_log.Info(std::format("hotkey: DIK {} -> VK {} (OG runtime)", a_scanCode, vk));
			return vk;
		}

		// Edge-triggered keyboard router for the panel hotkey. Appended to
		// MenuControls::handlers for the process lifetime (never removed;
		// F4SE plugins never unload).
		class InputSink final : public RE::BSInputEventUser
		{
		public:
			InputSink(Application::HotkeyController& hotkey, Core::ILogger& log, std::uint32_t a_matchCode, std::uint32_t a_configuredScanCode) :
				_hotkey(hotkey),
				_log(log),
				_matchCode(a_matchCode),
				_configuredScanCode(a_configuredScanCode)
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

				// C12 (diagnostic trace): edge-triggered per-physical-press log,
				// run before any filter so swallowed presses stay visible. The
				// static down-state is written from whichever thread delivers
				// input (C10 dumps showed more than one); a racy duplicate/lost
				// trace line is acceptable — deliberately no locks here.
				const auto code = static_cast<std::uint32_t>(a_event->QIDCode());
				const bool down = a_event->value != 0.0f;
				bool       freshPress{ false };
				if (a_event->device == RE::INPUT_DEVICE::kKeyboard && code < 256) {
					static std::array<bool, 256> downState{};
					freshPress = down && !downState[code];
					downState[code] = down;
					if (freshPress) {
						_log.Info(std::format("input: key down code={} justPressed={}", code, a_event->QJustPressed() ? 1 : 0));
					}
				}

				if (a_event->device != RE::INPUT_DEVICE::kKeyboard || !a_event->QJustPressed()) {
					return;
				}
				// C11: on OG the engine code is a VK (translated at init into
				// _matchCode); on NG/AE it is the raw DIK scan code. Only the
				// configured DIK code is ever handed down, so the Application
				// layer never sees VK values.
				if (code == _matchCode) {
					_log.Info(std::format("input: hotkey matched (code {}), toggling panel", code));
					_hotkey.OnKeyDown(_configuredScanCode);
				}
			}

		private:
			Application::HotkeyController& _hotkey;
			Core::ILogger&                 _log;
			std::uint32_t                  _matchCode;           // DIK on NG/AE, VK on OG
			std::uint32_t                  _configuredScanCode;  // always DIK (INI value)
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
			"settings: hotkey={} bufferSize={} fontSize={}",
			root.settings.hotkeyScanCode,
			root.settings.bufferSize,
			root.settings.fontSize));

		static Core::DialogueBuffer                buffer{ root.settings.bufferSize };
		static Infrastructure::PrismaViewBridge    bridge;
		static Application::ViewController         viewController(buffer, bridge);
		static Application::CapturePipeline        pipeline(buffer, viewController, root.log);
		static Application::HotkeyController       hotkey(root.settings, viewController);
		const auto                                 matchCode = ResolveHotkeyMatchCode(root.settings.hotkeyScanCode, root.log);
		static InputSink                           inputSink(hotkey, root.log, matchCode, root.settings.hotkeyScanCode);

		bridge.SetFontSize(root.settings.fontSize);
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

		root.graphReady = true;
		root.log.Info("data-loaded initialization complete (object graph built, subtitle hook installed)");
		return true;
	}
}
