#include "CapturePipeline.h"

#include <format>

#include "Core/Dialogue/PayloadBuilder.h"
#include "Application/View/ViewController.h"

namespace F4DH::Application
{
	CapturePipeline::CapturePipeline(Core::DialogueBuffer& buffer, ViewController& viewController, Core::ILogger& logger) :
		_buffer(buffer),
		_viewController(viewController),
		_logger(logger)
	{}

	void CapturePipeline::OnSubtitle(const Core::SubtitleEvent& event)
	{
		const bool accepted = Core::ConversationFilter::IsConversationDialogue(event);
		_logger.Debug(std::format(
			"subtitle: speaker=\"{}\" player={} menuOpen={} playerScene={} toPlayer={} -> {}",
			event.speakerName,
			event.speakerIsPlayer,
			event.menuOpen,
			event.sceneIsPlayerDialogue,
			event.spokenToPlayer,
			accepted ? "accepted" : "rejected"));
		if (!accepted) {
			return;
		}

		Core::DialogueLine line;
		line.kind = event.speakerIsPlayer ? Core::SpeakerKind::Player : Core::SpeakerKind::Npc;
		line.speaker = event.speakerIsPlayer ? "Player"
			: (event.speakerName.empty() ? "Unknown" : event.speakerName);
		line.text = event.text;

		_logger.Debug(std::format("recorded: {}: {}", line.speaker, line.text));

		_buffer.Push(std::move(line));
		_viewController.NotifyLine();
	}
}
