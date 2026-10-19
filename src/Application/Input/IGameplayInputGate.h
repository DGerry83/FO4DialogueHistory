#pragma once

namespace F4DH::Application
{
	// Abstracts blocking/unblocking gameplay user events (movement, camera,
	// ...) while the player types into a panel text input.
	class IGameplayInputGate
	{
	public:
		virtual ~IGameplayInputGate() = default;

		// Both idempotent; called only on the game thread.
		virtual void BlockGameplayInput() noexcept = 0;
		virtual void UnblockGameplayInput() noexcept = 0;
	};
}
