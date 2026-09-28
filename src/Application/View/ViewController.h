#pragma once

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

		// Mirrors the OnClearAll refresh: a bulk buffer mutation outside the
		// capture path (stress-test injector) needs a full snapshot re-push.
		void NotifyBulkChange();

		// Composition sets the instance the global thunks (close + clear)
		// route to (plain function pointers are instance-unaware).
		static void SetCloseTarget(ViewController* instance) noexcept;

		[[nodiscard]] ViewState State() const { return _state; }

	private:
		void Open();
		void Close();

		Core::DialogueBuffer& _buffer;
		IViewBridge&          _bridge;
		Core::ILogger&        _logger;
		ViewState             _state = ViewState::Hidden;
	};
}
