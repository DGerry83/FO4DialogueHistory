#pragma once

#include <string>

#include "Core/Note/NoteClassifier.h"
#include "Core/Note/NoteEntry.h"

namespace F4DH::Application
{
	struct NotePickupEvent
	{
		Core::NoteIdentity identity{};
		std::string title;
		Core::NoteSourceTraits traits{};
		Core::EncounterMetadata encounter{};
	};

	struct NoteReadEvent
	{
		Core::NoteIdentity identity{};
		std::string title;
		std::string plainBody;
		Core::NoteBodyMode bodyMode{ Core::NoteBodyMode::ResolveStatic };
		Core::NoteSourceTraits traits{};
		Core::EncounterMetadata encounter{};
	};

	class INoteSink
	{
	public:
		virtual ~INoteSink() = default;
		virtual void OnNotePickup(const NotePickupEvent& event) = 0;
		virtual void OnNoteRead(const NoteReadEvent& event) = 0;
	};
}
