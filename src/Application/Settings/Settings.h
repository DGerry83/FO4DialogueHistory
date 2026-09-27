#pragma once

#include <cstddef>
#include <cstdint>

namespace F4DH::Application
{
	// User-facing settings. Loaded once at plugin load; immutable afterwards.
	struct Settings
	{
		std::uint32_t hotkeyScanCode = 35;  // provisional: DIK_H — verified against vanilla binds in milestone 4 (AC9)
		std::size_t   bufferSize = 50;      // range 10–500
		int           fontSize = 16;        // range 10–32 (px)
	};
}
