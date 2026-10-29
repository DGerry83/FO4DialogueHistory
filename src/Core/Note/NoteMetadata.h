#pragma once

#include <cstdint>

namespace F4DH::Core
{
	[[nodiscard]] constexpr std::int32_t DisplayGameYear(std::int32_t a_engineYear) noexcept
	{
		return a_engineYear + 2000;
	}
}
