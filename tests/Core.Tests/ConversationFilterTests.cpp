#include <catch2/catch_test_macros.hpp>

#include "Core/Dialogue/ConversationFilter.h"

namespace
{
	F4DH::Core::SubtitleEvent MakeEvent(
		bool menuOpen,
		bool sceneIsPlayerDialogue,
		bool speakerIsPlayer,
		bool spokenToPlayer,
		std::int32_t questType = 0)
	{
		F4DH::Core::SubtitleEvent event;
		event.menuOpen = menuOpen;
		event.sceneIsPlayerDialogue = sceneIsPlayerDialogue;
		event.speakerIsPlayer = speakerIsPlayer;
		event.spokenToPlayer = spokenToPlayer;
		event.questType = questType;
		return event;
	}
}

TEST_CASE("full truth table over the four SubtitleEvent flags")
{
	struct Row
	{
		bool menuOpen;
		bool sceneIsPlayerDialogue;
		bool speakerIsPlayer;
		bool spokenToPlayer;
		bool expected;
	};

	// menuOpen, sceneIsPlayerDialogue, speakerIsPlayer, spokenToPlayer, expected
	const Row rows[] = {
		{ false, false, false, false, false },
		{ false, false, false, true, false },
		{ false, false, true, false, false },
		{ false, false, true, true, true },
		{ false, true, false, false, true },
		{ false, true, false, true, true },
		{ false, true, true, false, true },
		{ false, true, true, true, true },
		{ true, false, false, false, true },
		{ true, false, false, true, true },
		{ true, false, true, false, true },
		{ true, false, true, true, true },
		{ true, true, false, false, true },
		{ true, true, false, true, true },
		{ true, true, true, false, true },
		{ true, true, true, true, true },
	};

	for (std::size_t i = 0; i < std::size(rows); ++i) {
		const auto& row = rows[i];
		CAPTURE(i, row.menuOpen, row.sceneIsPlayerDialogue, row.speakerIsPlayer, row.spokenToPlayer);
		const auto event = MakeEvent(row.menuOpen, row.sceneIsPlayerDialogue, row.speakerIsPlayer, row.spokenToPlayer);
		REQUIRE(F4DH::Core::ConversationFilter::IsConversationDialogue(event) == row.expected);
	}
}

TEST_CASE("all-false bark-shaped event is rejected")
{
	REQUIRE_FALSE(F4DH::Core::ConversationFilter::IsConversationDialogue(MakeEvent(false, false, false, false)));
}

TEST_CASE("player speaker without spokenToPlayer is rejected")
{
	REQUIRE_FALSE(F4DH::Core::ConversationFilter::IsConversationDialogue(MakeEvent(false, false, true, false)));
}

TEST_CASE("spokenToPlayer alone with an NPC speaker is rejected")
{
	REQUIRE_FALSE(F4DH::Core::ConversationFilter::IsConversationDialogue(MakeEvent(false, false, false, true)));
}

TEST_CASE("player speaker addressing the player is accepted")
{
	REQUIRE(F4DH::Core::ConversationFilter::IsConversationDialogue(MakeEvent(false, false, true, true)));
}

TEST_CASE("NPC line owned by a typed quest is accepted with all gates false (C4)")
{
	// Scene-less staged quest performance shape: "How to HQ" (type=1 MainQuest)
	// field capture, 2026-09-28.
	REQUIRE(F4DH::Core::ConversationFilter::IsConversationDialogue(MakeEvent(false, false, false, false, 1)));
}

TEST_CASE("NPC line owned by a type-less bark-holder quest stays rejected")
{
	// DialogueGeneric / Conv* / AO_* holders are kNone — the bark case.
	REQUIRE_FALSE(F4DH::Core::ConversationFilter::IsConversationDialogue(MakeEvent(false, false, false, false, 0)));
}

TEST_CASE("player solo line from a typed holder quest stays rejected (user ruling 2026-09-28)")
{
	// "People comments about you" (type=6 Misc) solo player voice lines:
	// closed wontfix — the quest-type branch is scoped to non-player speakers.
	REQUIRE_FALSE(F4DH::Core::ConversationFilter::IsConversationDialogue(MakeEvent(false, false, true, false, 6)));
}
