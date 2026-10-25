#pragma once

#include <cstdint>
#include <vector>

#include "Core/Dialogue/DialogueBuffer.h"
#include "Core/ILogger.h"
#include "Core/Message/MessageBuffer.h"
#include "IViewBridge.h"

namespace F4DH::Application
{
	class IGameplayInputGate;

	enum class ViewState : std::uint8_t
	{
		Hidden,
		Open,
		Degraded,
	};

	// Owns the panel Hidden/Open/Degraded state machine and its data pushes.
	// Holds independent references to the dialogue and message buffers: the
	// two histories never share storage, snapshots, appends, or clear paths.
	class ViewController
	{
	public:
		ViewController(Core::DialogueBuffer& buffer, Core::MessageBuffer& messageBuffer, IViewBridge& bridge, Core::ILogger& logger);

		void Toggle();       // hotkey
		void NotifyLine();   // a new line was recorded; append if Open
		void NotifyMessage();  // a new message was recorded; append if Open
		void OnCloseRequested();  // Esc / JS closeRequested

		// Confirmed clear-history requests from the view (routed through
		// the bridge's clear callbacks). Message clear is its own channel —
		// it never touches dialogue storage and vice versa.
		void OnClearAll();
		void OnClearQuest(std::uint32_t questId);
		void OnClearQuests(const std::vector<std::uint32_t>& questIds);  // multi-quest selection clear: one pass per id, one snapshot
		void OnClearMessages();  // confirmed message-history clear: messages only

		// Mirrors the OnClearAll refresh: a bulk buffer mutation outside the
		// capture path (stress-test injector) needs a full snapshot re-push.
		void NotifyBulkChange();

		// Composition sets the instance the global thunks (close + clear)
		// route to (plain function pointers are instance-unaware).
		static void SetCloseTarget(ViewController* instance) noexcept;

		[[nodiscard]] ViewState State() const { return _state; }

		// #005: the view reports a text input focused/blurred through the
		// bridge's typingStateChanged listener. Written only from dispatched
		// game-thread tasks, read only on the game thread — plain bool by
		// design (documented threading invariant). Typing-active transitions
		// toggle the gameplay input gate (game thread too, by the same rule).
		void SetTyping(bool typing) noexcept;

		// Setter injection (no ctor churn); a null gate is a safe no-op. #005:
		// the gate blocks gameplay user events (movement/camera) while typing
		// is active — the MenuControls event swallow cannot reach the
		// PlayerControls user-event consumers.
		void SetGameplayInputGate(IGameplayInputGate* gate) noexcept { _gameplayGate = gate; }

		// True only while typing AND the panel is open — the open-state guard
		// is the second stuck-flag net next to the Open()/Close() resets.
		[[nodiscard]] bool IsTypingActive() const { return _typing && _state == ViewState::Open; }

	private:
		void Open();
		void Close();

		Core::DialogueBuffer& _buffer;
		Core::MessageBuffer&  _messageBuffer;
		IViewBridge&          _bridge;
		Core::ILogger&        _logger;
		ViewState             _state = ViewState::Hidden;
		bool                  _typing{ false };
		IGameplayInputGate*   _gameplayGate{ nullptr };  // #005 C2: null until Composition injects; no-op when null
	};
}
