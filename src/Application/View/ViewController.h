#pragma once

#include <cstdint>
#include <vector>

#include "Core/Dialogue/DialogueBuffer.h"
#include "Core/ILogger.h"
#include "IViewBridge.h"

namespace F4DH::Application
{
	enum class ViewState : std::uint8_t
	{
		Hidden,
		Open,
		Degraded,
	};

	// Owns the panel Hidden/Open/Degraded state machine and its data pushes.
	class ViewController
	{
	public:
		ViewController(Core::DialogueBuffer& buffer, IViewBridge& bridge, Core::ILogger& logger);

		void Toggle();       // hotkey
		void NotifyLine();   // a new line was recorded; append if Open
		void OnCloseRequested();  // Esc / JS closeRequested

		// Confirmed clear-history requests from the view (routed through
		// the bridge's clear callbacks).
		void OnClearAll();
		void OnClearQuest(std::uint32_t questId);
		void OnClearQuests(const std::vector<std::uint32_t>& questIds);  // multi-quest selection clear: one pass per id, one snapshot

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
		// design (documented threading invariant).
		void SetTyping(bool typing) noexcept { _typing = typing; }

		// True only while typing AND the panel is open — the open-state guard
		// is the second stuck-flag net next to the Open()/Close() resets.
		[[nodiscard]] bool IsTypingActive() const { return _typing && _state == ViewState::Open; }

	private:
		void Open();
		void Close();

		Core::DialogueBuffer& _buffer;
		IViewBridge&          _bridge;
		Core::ILogger&        _logger;
		ViewState             _state = ViewState::Hidden;
		bool                  _typing{ false };
	};
}
