#include "Composition.h"

#include <array>
#include <cstring>
#include <format>

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
#include "RE/C/Console.hpp"
#include "RE/L/LoadingMenu.hpp"
#include "RE/M/MainMenu.hpp"
#include "RE/M/MenuControls.hpp"
#include "RE/U/UI.hpp"

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

		// C10-DIAG (temporary): one-line hex dump of the first 0x40 bytes at the
		// event pointer (raw memcpy — no AV struct member reads, the whole point
		// is to compare engine bytes against the NG-lineage layout).
		[[nodiscard]] std::string HexDumpEvent(const RE::InputEvent* a_event)
		{
			std::array<std::byte, 0x40> raw{};
			std::memcpy(raw.data(), static_cast<const void*>(a_event), raw.size());

			auto dump = std::format("ev={:p}", static_cast<const void*>(a_event));
			for (std::size_t i = 0; i < raw.size(); ++i) {
				if (i % 16 == 0) {
					dump += " |";
				}
				dump += std::format(" {:02X}", std::to_integer<unsigned>(raw[i]));
			}
			dump += " |";
			return dump;
		}

		// C10-DIAG (temporary): true if any 4-byte-aligned dword in 0x28–0x3C
		// equals 35 (DIK_H) or 0x3F800000 (1.0f) — logs uncapped past the dump cap.
		[[nodiscard]] bool IsHotkeyCandidate(const RE::InputEvent* a_event)
		{
			std::array<std::byte, 0x40> raw{};
			std::memcpy(raw.data(), static_cast<const void*>(a_event), raw.size());
			for (std::size_t off = 0x28; off <= 0x3C; off += 4) {
				std::uint32_t dword{ 0 };
				std::memcpy(&dword, raw.data() + off, sizeof(dword));
				if (dword == 35u || dword == 0x3F800000u) {
					return true;
				}
			}
			return false;
		}

		// Edge-triggered keyboard router for the panel hotkey. Appended to
		// MenuControls::handlers for the process lifetime (never removed;
		// F4SE plugins never unload).
		class InputSink final : public RE::BSInputEventUser
		{
		public:
			InputSink(Application::HotkeyController& hotkey, Core::ILogger& log, std::uint32_t a_hotkeyScanCode) :
				_hotkey(hotkey),
				_log(log),
				_hotkeyScanCode(a_hotkeyScanCode)
			{}

			// C10-DIAG (temporary): accept ALL events and raw-dump each one
			// (capped; hotkey-candidate dwords dump uncapped) to pin the OG
			// ButtonEvent layout. C9's opt-in comment kept for the fix commit.
			bool ShouldHandleEvent(const RE::InputEvent* a_event) override
			{
				if (a_event) {
					static std::uint32_t dumpCount{ 0 };
					if (IsHotkeyCandidate(a_event)) {
						_log.Info(std::format("input: HOTKEY-CANDIDATE {}", HexDumpEvent(a_event)));
					} else if (dumpCount < 300) {
						++dumpCount;
						_log.Info(std::format("input: dump #{} {}", dumpCount, HexDumpEvent(a_event)));
					}
					if (!_loggedFirstEvent) {
						_loggedFirstEvent = true;
						_log.Info("input: first input event received");
					}
				}
				return true;
			}

			void OnButtonEvent(const RE::ButtonEvent* a_event) override
			{
				// C10-DIAG (temporary): raw dump plus AV-interpreted field reads,
				// logged before any early-return, to compare against the bytes.
				if (a_event) {
					_log.Info(std::format(
						"input: OnButtonEvent {} device={} eventType={} idCode={} value={} held={} justPressed={}",
						HexDumpEvent(a_event),
						std::to_underlying(a_event->device.get()),
						std::to_underlying(a_event->eventType.get()),
						a_event->QIDCode(),
						a_event->value,
						a_event->heldDownSecs,
						a_event->QJustPressed()));
				}
				if (!a_event || a_event->device != RE::INPUT_DEVICE::kKeyboard || !a_event->QJustPressed()) {
					return;
				}
				// Inert while the console, main menu, or loading screen is up.
				if (const auto ui = RE::UI::GetSingleton()) {
					if (ui->IsMenuOpen<RE::Console>() ||
						ui->IsMenuOpen<RE::MainMenu>() ||
						ui->IsMenuOpen<RE::LoadingMenu>()) {
						return;
					}
				}
				const auto scanCode = static_cast<std::uint32_t>(a_event->QIDCode());
				if (scanCode == _hotkeyScanCode) {
					_log.Info(std::format("input: hotkey matched (scanCode {}), toggling panel", scanCode));
				}
				_hotkey.OnKeyDown(scanCode);
			}

		private:
			Application::HotkeyController& _hotkey;
			Core::ILogger&                 _log;
			std::uint32_t                  _hotkeyScanCode;
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
		static InputSink                           inputSink(hotkey, root.log, root.settings.hotkeyScanCode);

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
