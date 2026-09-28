#pragma once

#include "Application/Settings/Settings.h"
#include "Core/Dialogue/DialogueBuffer.h"
#include "Core/ILogger.h"

namespace F4DH::Application
{
	class ViewController;

	// Single responsibility: inject one synthetic batch into the real dialogue
	// buffer on demand (diagnostic stress testing of the view pipeline).
	// Injected lines take the exact same path as captured ones — buffer push,
	// then a full snapshot refresh when the panel is Open. Runs on the
	// input-event thread with bounded allocation: at most
	// Settings::stressTestLines lines (clamped to 5000) per press.
	class StressTestInjector
	{
		public:
			StressTestInjector(Core::DialogueBuffer& buffer, ViewController& viewController, Core::ILogger& logger, const Settings& settings);

			// Generates one batch per the Settings counts and pushes it into
			// the buffer; refreshes the view once if the panel is Open.
			void InjectBatch();

		private:
			Core::DialogueBuffer& _buffer;
			ViewController&       _viewController;
			Core::ILogger&        _logger;
			const Settings&       _settings;
	};
}
