#pragma once

#include "Core/Dialogue/DialogueBuffer.h"
#include "Core/Dialogue/ConversationFilter.h"
#include "Core/ILogger.h"
#include "ISubtitleSink.h"

namespace F4DH::Application
{
	class ViewController;

	// Routes each subtitle event through the filter into the buffer and
	// notifies the view controller of accepted lines. Logs every filter
	// verdict so capture behavior is runtime-verifiable from the F4SE log.
	class CapturePipeline final : public ISubtitleSink
	{
	public:
		CapturePipeline(Core::DialogueBuffer& buffer, ViewController& viewController, Core::ILogger& logger);

		void OnSubtitle(const Core::SubtitleEvent& event) override;

	private:
		Core::DialogueBuffer& _buffer;
		ViewController&       _viewController;
		Core::ILogger&        _logger;
	};
}
