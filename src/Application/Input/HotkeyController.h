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

		private:
			const Settings&     _settings;
			ViewController&     _viewController;
			StressTestInjector& _stressTestInjector;
	};
}
