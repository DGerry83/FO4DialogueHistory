#include "MessageCapturePipeline.h"

#include <format>
#include <utility>

#include "Application/View/ViewController.h"
#include "Core/Message/MessageContentFilter.h"

namespace F4DH::Application
{
	MessageCapturePipeline::MessageCapturePipeline(Core::MessageBuffer& buffer, ViewController& viewController, Core::ILogger& logger, bool verboseCapture, std::vector<std::string> messageExcludedPrefixes) :
		_buffer(buffer),
		_viewController(viewController),
		_logger(logger),
		_verboseCapture(verboseCapture),
		_messageExcludedPrefixes(std::move(messageExcludedPrefixes))
	{}

	void MessageCapturePipeline::OnMessage(const MessageEvent& event)
	{
		const auto excludedPrefix = Core::MessageContentFilter::FindExcludedPrefix(event.text, _messageExcludedPrefixes);
		const bool accepted = !event.text.empty() && excludedPrefix.empty();
		if (_verboseCapture) {
			if (!excludedPrefix.empty()) {
				_logger.Info(std::format(
					"message: warning={} radio={} throttle={} -> rejected (prefix '{}') | {}",
					event.isWarningMessage,
					event.isRadioMessage,
					event.canBeThrottled,
					excludedPrefix,
					event.text.substr(0, 60)));
			} else {
				_logger.Info(std::format(
					"message: warning={} radio={} throttle={} -> {} | {}",
					event.isWarningMessage,
					event.isRadioMessage,
					event.canBeThrottled,
					accepted ? "accepted" : "rejected",
					event.text.substr(0, 60)));
			}
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
