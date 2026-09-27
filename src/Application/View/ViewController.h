#pragma once

#include "Core/Dialogue/DialogueBuffer.h"
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
		ViewController(Core::DialogueBuffer& buffer, IViewBridge& bridge);

		void Toggle();       // hotkey
		void NotifyLine();   // a new line was recorded; append if Open
		void OnCloseRequested();  // Esc / JS closeRequested

		[[nodiscard]] ViewState State() const { return _state; }

	private:
		void Open();
		void Close();

		Core::DialogueBuffer& _buffer;
		IViewBridge&          _bridge;
		ViewState             _state = ViewState::Hidden;
	};
}
