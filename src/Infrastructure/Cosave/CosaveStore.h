#pragma once

namespace F4DH::Core
{
	class DialogueBuffer;
	class MessageBuffer;
	class NoteStore;
}

namespace F4DH::Infrastructure
{
	// F4SE cosave persistence for dialogue, messages, and notes. Registered at
	// plugin load and bound at game-data-ready. Save writes independent HIST,
	// MSGS, and NOTE records; load clears all stores, scans every record in any
	// order, remaps saved note form IDs, and isolates malformed records to
	// their own history. Revert clears all three stores.
	namespace CosaveStore
	{
		void Install();
		void Bind(Core::DialogueBuffer* a_buffer, Core::MessageBuffer* a_messageBuffer, Core::NoteStore* a_noteStore);
	}
}
