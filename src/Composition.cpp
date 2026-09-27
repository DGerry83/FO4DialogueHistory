#include "Composition.h"

#include "Application/Capture/CapturePipeline.h"
#include "Application/Input/HotkeyController.h"
#include "Application/Settings/Settings.h"
#include "Application/View/ViewController.h"
#include "Core/Dialogue/DialogueBuffer.h"
#include "Infrastructure/Hooks/SubtitleHook.h"
#include "Infrastructure/Logging/Log.h"
#include "Infrastructure/Prisma/PrismaViewBridge.h"

#include "RE/B/BSInputEventUser.h"
#include "RE/B/ButtonEvent.h"
#include "RE/C/Console.h"
#include "RE/L/LoadingMenu.h"
#include "RE/M/MainMenu.h"
#include "RE/M/MenuControls.h"
#include "RE/U/UI.h"

// Process-lifetime object graph, held by function-local statics (F4SE
// plugins never unload). M1: log + settings + buffer. C3: view bridge (stub
// until C4), view controller, capture pipeline, subtitle hook. C5 adds the
// INI-backed settings store.

namespace F4DH
{
	namespace
	{
		struct CompositionRoot
		{
			Infrastructure::Log   log;
			Application::Settings settings;                    // shipped defaults; INI arrives in C5
			Core::DialogueBuffer  buffer{ settings.bufferSize };
			bool                  hookInstalled{ false };
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
		auto& root = Root();

		// C4 wires the real PrismaUI bridge; all components are process-
		// lifetime statics constructed on first call.
		static Infrastructure::PrismaViewBridge bridge;
		static Application::ViewController      viewController(root.buffer, bridge);
		static Application::CapturePipeline     pipeline(root.buffer, viewController, root.log);
		static Application::HotkeyController    hotkey(root.settings, viewController);
		static InputSink                        inputSink(hotkey);

		if (root.hookInstalled) {
			root.log.Info("post-load initialization already complete");
			return true;
		}

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

		root.hookInstalled = true;
		root.log.Info("post-load initialization complete (subtitle hook installed)");
		return true;
	}

	bool Composition::InitializeDataLoaded()
	{
		Root().log.Info("data-loaded initialization complete");
		return true;
	}
}
