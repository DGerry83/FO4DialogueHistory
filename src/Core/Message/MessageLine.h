#pragma once

#include <string>

namespace F4DH::Core
{
	// One accepted-for-display HUD message. Value object: identity is
	// positional within MessageBuffer; only the resolved text is kept.
	struct MessageLine
	{
		std::string text;  // resolved HUD message text (UTF-8)
	};
}
