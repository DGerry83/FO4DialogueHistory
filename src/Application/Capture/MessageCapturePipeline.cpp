#include "MessageCapturePipeline.h"

#include <format>

#include "Application/View/ViewController.h"
#include "Core/Message/MessageContentFilter.h"

namespace F4DH::Application
{
	MessageCapturePipeline::MessageCapturePipeline(Core::MessageBuffer& buffer, ViewController& viewController, Core::ILogger& logger, bool verboseCapture) :
		_buffer(buffer),
		_viewController(viewController),
		_logger(logger),
		_verboseCapture(verboseCapture)
	{}

	void MessageCapturePipeline::OnMessage(const MessageEvent& event)
	{
		const bool accepted = Core::MessageContentFilter::ShouldCapture(event.text);
		if (_verboseCapture) {
			_logger.Info(std::format(
				"message: warning={} radio={} throttle={} -> {} | {}",
				event.isWarningMessage,
				event.isRadioMessage,
				event.canBeThrottled,
				accepted ? "accepted" : "rejected",
				event.text.substr(0, 60)));
		}
		if (!accepted) {
			return;
		}

		Core::MessageLine message;
		message.text = event.text;

		_logger.Debug(std::format("recorded message: {}", message.text));

		_buffer.Push(std::move(message));
		_viewController.NotifyMessage();
	}
}
