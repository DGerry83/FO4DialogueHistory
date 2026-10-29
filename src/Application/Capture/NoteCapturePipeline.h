#pragma once

#include "Core/ILogger.h"
#include "Core/Note/NoteStore.h"
#include "INoteSink.h"

namespace F4DH::Application
{
	class ViewController;

	class NoteCapturePipeline final : public INoteSink
	{
	public:
		NoteCapturePipeline(Core::NoteStore& store, ViewController& viewController, Core::ILogger& logger, bool verboseCapture);

		void OnNotePickup(const NotePickupEvent& event) override;
		void OnNoteRead(const NoteReadEvent& event) override;

	private:
		Core::NoteStore& _store;
		ViewController& _viewController;
		Core::ILogger& _logger;
		bool _verboseCapture;
	};
}
