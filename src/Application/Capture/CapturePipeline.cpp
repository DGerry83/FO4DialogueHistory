#include "CapturePipeline.h"

#include <format>

#include "Core/Dialogue/PayloadBuilder.h"
#include "Application/View/ViewController.h"

namespace F4DH::Application
{
	CapturePipeline::CapturePipeline(Core::DialogueBuffer& buffer, ViewController& viewController, Core::ILogger& logger, bool verboseCapture) :
		_buffer(buffer),
		_viewController(viewController),
		_logger(logger),
		_verboseCapture(verboseCapture)
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
		if (_verboseCapture) {
			_logger.Info(std::format(
				"subtitle: speaker=\"{}\" player={} menuOpen={} playerScene={} toPlayer={} hasScene={} quest=\"{}\" id={:08X} type={} -> {} | {}",
				event.speakerName,
				event.speakerIsPlayer,
				event.menuOpen,
				event.sceneIsPlayerDialogue,
				event.spokenToPlayer,
				event.hasScene,
				event.questName,
				event.questId,
				event.questType,
				accepted ? "accepted" : "rejected",
				event.text.substr(0, 60)));
		}
		if (!accepted) {
			return;
		}

		Core::DialogueLine line;
		line.kind = event.speakerIsPlayer ? Core::SpeakerKind::Player : Core::SpeakerKind::Npc;
		line.speaker = event.speakerIsPlayer ? "Player"
			: (event.speakerName.empty() ? "Unknown" : event.speakerName);
		line.text = event.text;
		line.questId = event.questId;
		line.questName = event.questName;
		line.questType = static_cast<std::uint8_t>(event.questType);

		_logger.Debug(std::format("recorded: {}: {}", line.speaker, line.text));

		_buffer.Push(std::move(line));
		_viewController.NotifyLine();
	}
}
