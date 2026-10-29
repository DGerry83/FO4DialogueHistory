#include "NoteCapturePipeline.h"

#include <format>

#include "Application/View/ViewController.h"

namespace F4DH::Application
{
	NoteCapturePipeline::NoteCapturePipeline(Core::NoteStore& store, ViewController& viewController, Core::ILogger& logger, bool verboseCapture) :
		_store(store), _viewController(viewController), _logger(logger), _verboseCapture(verboseCapture)
	{}

	void NoteCapturePipeline::OnNotePickup(const NotePickupEvent& event)
	{
		if (!Core::NoteClassifier::IsDocument(event.traits)) {
			if (_verboseCapture) _logger.Info(std::format("note: pickup rejected base=0x{:08X}", event.identity.baseFormID));
			return;
		}
		const auto change = _store.ObservePickup(event.identity, event.title, event.encounter);
		if (_verboseCapture) {
			_logger.Info(std::format("note: pickup base=0x{:08X} entry={} (pending)", event.identity.baseFormID, change.entryId));
		}
	}

	void NoteCapturePipeline::OnNoteRead(const NoteReadEvent& event)
	{
		if (!Core::NoteClassifier::IsDocument(event.traits)) {
			if (_verboseCapture) _logger.Info(std::format("note: read rejected base=0x{:08X}", event.identity.baseFormID));
			return;
		}
		const auto change = _store.ObserveRead(event.identity, event.title, event.plainBody, event.bodyMode, event.encounter);
		if (_verboseCapture) {
			_logger.Info(std::format("note: read base=0x{:08X} entry={} visible={}", event.identity.baseFormID, change.entryId, change.visibleChanged));
		}
		if (change.visibleChanged) _viewController.NotifyNote();
	}
}
