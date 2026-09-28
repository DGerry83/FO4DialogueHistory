#pragma once

#include "Core/Dialogue/DialogueBuffer.h"
#include "Core/Dialogue/ConversationFilter.h"
#include "Core/ILogger.h"
#include "ISubtitleSink.h"

namespace F4DH::Application
{
	class ViewController;

	// Routes each subtitle event through the filter into the buffer and
	// notifies the view controller of accepted lines. When verbose capture is
	// enabled, logs every filter verdict at Information level (accepted and
	// rejected alike) so capture behavior is runtime-verifiable from the log.
	class CapturePipeline final : public ISubtitleSink
	{
	public:
		CapturePipeline(Core::DialogueBuffer& buffer, ViewController& viewController, Core::ILogger& logger, bool verboseCapture);

		void OnSubtitle(const Core::SubtitleEvent& event) override;

	private:
		Core::DialogueBuffer& _buffer;
		ViewController&       _viewController;
		Core::ILogger&        _logger;
		const bool            _verboseCapture;
	};
}
