#pragma once

#include "Core/Message/MessageBuffer.h"
#include "Core/ILogger.h"
#include "IMessageSink.h"

namespace F4DH::Application
{
	class ViewController;

	// Routes each accepted-for-display HUD message into the message buffer and
	// notifies the view controller of accepted entries. Empty text is rejected
	// here (the buffer itself stores unconditionally by design). When verbose
	// capture is enabled, logs every message verdict at Information level
	// (accepted and rejected alike) with the display flags available at the
	// capture boundary, so message capture behavior is runtime-verifiable
	// from the log.
	class MessageCapturePipeline final : public IMessageSink
	{
	public:
		MessageCapturePipeline(Core::MessageBuffer& buffer, ViewController& viewController, Core::ILogger& logger, bool verboseCapture);

		void OnMessage(const MessageEvent& event) override;

	private:
		Core::MessageBuffer& _buffer;
		ViewController&      _viewController;
		Core::ILogger&       _logger;
		const bool           _verboseCapture;
	};
}
