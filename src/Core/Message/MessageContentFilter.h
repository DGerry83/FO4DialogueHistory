#pragma once

#include <string_view>

namespace F4DH::Core
{
	class MessageContentFilter
	{
	public:
		[[nodiscard]] static bool ShouldCapture(std::string_view a_text) noexcept
		{
			return !a_text.empty() && !a_text.starts_with(kHudFrameworkProtocolPrefix);
		}

	private:
		static constexpr std::string_view kHudFrameworkProtocolPrefix{ "|HUDFramework|" };
	};
}
