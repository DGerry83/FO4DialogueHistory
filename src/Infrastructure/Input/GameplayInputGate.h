#pragma once

#include "Application/Input/IGameplayInputGate.h"
#include "Core/ILogger.h"

#include "RE/B/BSInputEnableLayer.hpp"
#include "RE/B/BSInputEnableManager.hpp"

namespace F4DH::Infrastructure
{
	// #005: blocks gameplay user events (WASD movement, camera, ...) through
	// BSInputEnableManager — the same mechanism DisablePlayerControls uses —
	// while the player types into a panel text input. PlayerControls consumers
	// never consult the MenuControls handler chain, so the C1 event swallow
	// could not cover them. One layer is allocated once at data-loaded; the
	// toggle path is pure EnableUserEvent calls, zero allocation (game-thread
	// rule). The smart pointer is held for the process lifetime — the layer's
	// IncRef/DecRef is the lifetime management, never freed manually.
	class GameplayInputGate final : public Application::IGameplayInputGate
	{
	public:
		// enabled = [Settings] ExclusiveTypingInput; false leaves the gate inert.
		GameplayInputGate(Core::ILogger& log, bool enabled);

		// Allocates the BSInputEnableManager layer (game thread, data ready).
		// Failure logs a Warn and leaves the gate an inert no-op — never fatal.
		void Initialize() noexcept;

		void BlockGameplayInput() noexcept override;
		void UnblockGameplayInput() noexcept override;

	private:
		void SetBlocked(bool block) noexcept;

		Core::ILogger&                              _log;
		bool                                        _enabled;
		RE::BSInputEnableManager*                   _manager{ nullptr };  // cached at Initialize; process-lifetime singleton
		RE::BSTSmartPointer<RE::BSInputEnableLayer> _layer;               // held for the process lifetime
		bool                                        _blocked{ false };
	};
}
