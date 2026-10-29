#pragma once

namespace F4DH::Core
{
	struct NoteSourceTraits
	{
		bool teaches{ false };
		bool hasMagazineScript{ false };
		bool hasPerkMagKeyword{ false };
		bool textureOnly{ false };
	};

	class NoteClassifier
	{
	public:
		[[nodiscard]] static constexpr bool IsDocument(const NoteSourceTraits& a_traits) noexcept
		{
			return !a_traits.teaches && !a_traits.hasMagazineScript && !a_traits.hasPerkMagKeyword && !a_traits.textureOnly;
		}
	};
}
