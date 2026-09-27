#include "Composition.h"

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

#include "RE/B/BSInputEventUser.h"
#include "RE/B/ButtonEvent.h"
#include "RE/C/Console.h"
#include "RE/L/LoadingMenu.h"
#include "RE/M/MainMenu.h"
#include "RE/M/MenuControls.h"
#include "RE/U/UI.h"

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

		// Edge-triggered keyboard router for the panel hotkey. Registered
		// with MenuControls for the process lifetime (the fork has no
		// unregister; F4SE plugins never unload).
		class InputSink final : public RE::BSInputEventUser
		{
		public:
			explicit InputSink(Application::HotkeyController& hotkey) :
				_hotkey(hotkey)
			{}

			void OnButtonEvent(const RE::ButtonEvent* a_event) override
			{
				if (!a_event || a_event->device != RE::INPUT_DEVICE::kKeyboard || !a_event->QJustPressed()) {
					return;
				}
				// Inert while the console, main menu, or loading screen is up.
				if (const auto ui = RE::UI::GetSingleton()) {
					if (ui->GetMenuOpen<RE::Console>() ||
						ui->GetMenuOpen<RE::MainMenu>() ||
						ui->GetMenuOpen<RE::LoadingMenu>()) {
						return;
					}
				}
				_hotkey.OnKeyDown(a_event->QIDCode());
			}

		private:
			Application::HotkeyController& _hotkey;
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
		static InputSink                           inputSink(hotkey);

		bridge.SetFontSize(root.settings.fontSize);
		Application::ViewController::SetCloseTarget(&viewController);

		if (const auto menuControls = RE::MenuControls::GetSingleton()) {
			menuControls->RegisterHandler(&inputSink);
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
