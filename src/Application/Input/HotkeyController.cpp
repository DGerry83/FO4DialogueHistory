#include "HotkeyController.h"

#include "Application/View/ViewController.h"

namespace F4DH::Application
{
	HotkeyController::HotkeyController(const Settings& settings, ViewController& viewController) :
		_settings(settings),
		_viewController(viewController)
	{}

	void HotkeyController::OnKeyDown(std::uint32_t scanCode)
	{
		if (scanCode == _settings.hotkeyScanCode) {
			_viewController.Toggle();
		}
	}
}
