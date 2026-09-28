#include "HotkeyController.h"

#include "Application/StressTest/StressTestInjector.h"
#include "Application/View/ViewController.h"

namespace F4DH::Application
{
	HotkeyController::HotkeyController(const Settings& settings, ViewController& viewController, StressTestInjector& stressTestInjector) :
		_settings(settings),
		_viewController(viewController),
		_stressTestInjector(stressTestInjector)
	{}

	void HotkeyController::OnKeyDown(std::uint32_t scanCode)
	{
		if (scanCode == _settings.hotkeyScanCode) {
			_viewController.Toggle();
		} else if (_settings.stressTestKey != 0 && scanCode == _settings.stressTestKey) {
			_stressTestInjector.InjectBatch();
		}
	}
}
