#pragma once

#include "Application/Settings/Settings.h"

namespace F4DH::Application
{
	class ViewController;

	// Translates edge-triggered key presses into panel toggle requests.
	class HotkeyController
	{
	public:
		HotkeyController(const Settings& settings, ViewController& viewController);

		// Called by the F4SE input event sink for each key-down event.
		void OnKeyDown(std::uint32_t scanCode);

	private:
		const Settings&  _settings;
		ViewController&  _viewController;
	};
}
