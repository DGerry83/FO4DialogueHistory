#pragma once

#include "Core/Note/NoteEntry.h"

namespace F4DH::Application
{
	class INoteBodySource
	{
	public:
		virtual ~INoteBodySource() = default;
		[[nodiscard]] virtual Core::NoteBodyResult ResolveBody(const Core::NoteRecord& record) = 0;
	};
}
