#pragma once

#include "Core/Dialogue/DialogueBuffer.h"
#include "Core/Dialogue/ConversationFilter.h"
#include "ISubtitleSink.h"

namespace F4DH::Application
{
	class ViewController;

	// Routes each subtitle event through the filter into the buffer and
	// notifies the view controller of accepted lines.
	class CapturePipeline final : public ISubtitleSink
	{
	public:
		CapturePipeline(Core::DialogueBuffer& buffer, ViewController& viewController);

		void OnSubtitle(const Core::SubtitleEvent& event) override;

	private:
		Core::DialogueBuffer& _buffer;
		ViewController&       _viewController;
	};
}
