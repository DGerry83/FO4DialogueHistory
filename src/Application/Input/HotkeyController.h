#pragma once

#include "Application/Settings/Settings.h"

namespace F4DH::Application
{
	class ViewController;
	class StressTestInjector;

	// Translates edge-triggered key presses into panel toggle requests or
	// stress-test batch injections (router only — no capture-path work here).
	class HotkeyController
	{
		public:
			HotkeyController(const Settings& settings, ViewController& viewController, StressTestInjector& stressTestInjector);

			// Called by the F4SE input event sink for each key-down event.
			void OnKeyDown(std::uint32_t scanCode);

			// Called by the input sink when the configured gamepad binding
			// matches (the sink already resolved the binding — no code
			// argument). Same toggle path as OnKeyDown's hotkey branch.
			void OnGamepadButton();

			// #005: true while a panel text input owns typing (and the panel
			// is open) — the input sink gates its keyboard routing and
			// consumption on this. Forwards to the ViewController.
			[[nodiscard]] bool IsTypingActive() const;

			// #007: true while the panel is open — the input sink suspends
			// gamepad chord matching then (the view owns the controller: B /
			// Start close; PrismaUI's V12 action capture swallows modifier
			// edges while focused, so edge-tracked chord state is unreliable
			// and the modifier button is overloaded with view actions).
			[[nodiscard]] bool IsPanelOpen() const;

		private:
			const Settings&     _settings;
			ViewController&     _viewController;
			StressTestInjector& _stressTestInjector;
	};
}
