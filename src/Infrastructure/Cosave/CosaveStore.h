#pragma once

namespace F4DH::Core
{
	class DialogueBuffer;
	class MessageBuffer;
}

namespace F4DH::Infrastructure
{
	// F4SE cosave persistence for the dialogue and message buffers. Registered
	// at plugin load (F4SEPlugin_Load) — SetUniqueID is mandatory, without it
	// F4SE silently skips the save callback. The serialization callbacks run
	// synchronously on the main game thread inside the Papyrus VM save/load
	// path — the same thread as the capture hooks, which serializes access —
	// so no locks are taken anywhere in this module. Both buffers are bound
	// later, at game-data-ready; until then every callback no-ops on null.
	// Save writes the HIST and MSGS records independently; load clears both
	// buffers, scans ALL records in any order, and a malformed record fails
	// closed only for its own history. SetFormDeleteCallback is deliberately
	// not registered (dead per the F4SE header comment).
	namespace CosaveStore
	{
		void Install();
		void Bind(Core::DialogueBuffer* a_buffer, Core::MessageBuffer* a_messageBuffer);
	}
}
