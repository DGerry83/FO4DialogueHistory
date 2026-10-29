#pragma once

#include <string>
#include <string_view>

namespace F4DH::Core
{
	class NoteTextFormatter
	{
	public:
		[[nodiscard]] static std::string ToPlainText(std::string_view a_text);
		[[nodiscard]] static std::string NormalizeOpening(std::string_view a_text);
		[[nodiscard]] static bool ContainsUnresolvedAlias(std::string_view a_text) noexcept;
	};
}
