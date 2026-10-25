#pragma once

#include <string>

namespace F4DH::Application
{
	// One observed HUD display-data occurrence, translated from the engine
	// event by the Infrastructure sink. Carries the resolved text plus the
	// display flags available at the accepted-display boundary (diagnostics
	// only — no flag ever filters capture). Application-owned: Core's
	// MessageLine stays {text}-only.
	struct MessageEvent
	{
		std::string text;
		bool        isWarningMessage{ false };
		bool        isRadioMessage{ false };
		bool        canBeThrottled{ false };
	};

	// Receives translated HUD message display events from the capture sink.
	class IMessageSink
	{
	public:
		virtual ~IMessageSink() = default;

		virtual void OnMessage(const MessageEvent& event) = 0;
	};
}
