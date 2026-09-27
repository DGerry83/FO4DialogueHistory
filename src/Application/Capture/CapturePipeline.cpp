#include "CapturePipeline.h"

#include "Core/Dialogue/PayloadBuilder.h"
#include "Application/View/ViewController.h"

namespace F4DH::Application
{
	CapturePipeline::CapturePipeline(Core::DialogueBuffer& buffer, ViewController& viewController) :
		_buffer(buffer),
		_viewController(viewController)
	{}

	void CapturePipeline::OnSubtitle(const Core::SubtitleEvent& event)
	{
		if (!Core::ConversationFilter::IsConversationDialogue(event)) {
			return;
		}

		Core::DialogueLine line;
		line.kind = event.speakerIsPlayer ? Core::SpeakerKind::Player : Core::SpeakerKind::Npc;
		line.speaker = event.speakerIsPlayer ? "Player"
			: (event.speakerName.empty() ? "Unknown" : event.speakerName);
		line.text = event.text;

		_buffer.Push(std::move(line));
		_viewController.NotifyLine();
	}
}
