#include "StressTestInjector.h"

#include <format>
#include <utility>

#include "Application/View/ViewController.h"
#include "Core/Dialogue/StressTestDataGenerator.h"

namespace F4DH::Application
{
	StressTestInjector::StressTestInjector(Core::DialogueBuffer& buffer, ViewController& viewController, Core::ILogger& logger, const Settings& settings) :
		_buffer(buffer),
		_viewController(viewController),
		_logger(logger),
		_settings(settings)
	{}

	void StressTestInjector::InjectBatch()
	{
		auto lines = Core::StressTestDataGenerator::Generate(_settings.stressTestLines, _settings.stressTestQuests);
		const auto injected = lines.size();
		for (auto& line : lines) {
			_buffer.Push(std::move(line));
		}
		_viewController.NotifyBulkChange();
		_logger.Info(std::format("stress test: injected {} line(s) across {} synthetic quest(s); buffer now holds {} line(s)",
			injected, _settings.stressTestQuests, _buffer.Size()));
	}
}
