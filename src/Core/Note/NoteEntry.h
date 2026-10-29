#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace F4DH::Core
{
	enum class NoteIdentityKind : std::uint8_t
	{
		StaticBase = 0,
		DynamicReference = 1,
		DynamicSynthetic = 2,
	};

	enum class NoteState : std::uint8_t
	{
		PendingPickup = 0,
		Read = 1,
	};

	enum class NoteBodyMode : std::uint8_t
	{
		ResolveStatic = 0,
		CachedResolved = 1,
		Empty = 2,
		Unavailable = 3,
	};

	enum class EncounterKind : std::uint8_t
	{
		Pickup = 0,
		Read = 1,
	};

	struct GameTimestamp
	{
		std::int32_t year{ 0 };
		std::uint8_t month{ 0 };
		std::uint8_t day{ 0 };
		std::uint8_t hour{ 0 };
		std::uint8_t minute{ 0 };
	};

	struct EncounterMetadata
	{
		EncounterKind kind{ EncounterKind::Read };
		GameTimestamp timestamp{};
		std::string location;
	};

	struct NoteIdentity
	{
		std::uint64_t entryId{ 0 };
		NoteIdentityKind kind{ NoteIdentityKind::StaticBase };
		std::uint32_t baseFormID{ 0 };
		std::uint32_t sourceRefFormID{ 0 };
		std::uint32_t localBaseFormID{ 0 };
		std::uint32_t instanceOrdinal{ 0 };
		std::uint32_t runtimeUniqueID{ 0 };
		std::string pluginName;
	};

	struct NoteRecord
	{
		NoteIdentity identity{};
		std::string title;
		std::uint64_t firstReadSequence{ 0 };
		NoteState state{ NoteState::PendingPickup };
		NoteBodyMode bodyMode{ NoteBodyMode::ResolveStatic };
		std::string cachedBody;
		std::optional<EncounterMetadata> firstEncounter;
	};

	struct NoteListItem
	{
		std::uint64_t entryId{ 0 };
		std::string title;
		std::uint64_t firstReadSequence{ 0 };
	};

	struct NoteBodyResult
	{
		enum class Status : std::uint8_t
		{
			Ready = 0,
			MissingPlugin = 1,
			Empty = 2,
			Unavailable = 3,
		};

		std::uint64_t entryId{ 0 };
		Status status{ Status::Unavailable };
		std::string title;
		std::string body;
		std::string pluginName;
		std::optional<EncounterMetadata> firstEncounter;
	};

	struct NoteChange
	{
		std::uint64_t entryId{ 0 };
		bool visibleChanged{ false };
	};
}
