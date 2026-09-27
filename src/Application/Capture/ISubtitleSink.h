#pragma once

#include "Core/Dialogue/SubtitleEvent.h"

namespace F4DH::Application
{
	// Receives translated subtitle events from the capture hook.
	class ISubtitleSink
	{
	public:
		virtual ~ISubtitleSink() = default;

		virtual void OnSubtitle(const Core::SubtitleEvent& event) = 0;
	};
}
