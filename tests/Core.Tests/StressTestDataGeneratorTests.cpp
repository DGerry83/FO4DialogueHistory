#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>
#include <vector>

#include "Core/Dialogue/PayloadBuilder.h"
#include "Core/Dialogue/StressTestDataGenerator.h"

namespace
{
	std::set<std::uint32_t> DistinctNonzeroQuestIds(const std::vector<F4DH::Core::DialogueLine>& lines)
	{
		std::set<std::uint32_t> ids;
		for (const auto& line : lines) {
			if (line.questId != 0) {
				ids.insert(line.questId);
			}
		}
		return ids;
	}
}

TEST_CASE("generate returns exactly N lines spanning exactly M distinct synthetic questIds")
{
	const auto batch = F4DH::Core::StressTestDataGenerator::Generate(500, 20);
	REQUIRE(batch.size() == 500);
	REQUIRE(DistinctNonzeroQuestIds(batch).size() == 20);

	const auto single = F4DH::Core::StressTestDataGenerator::Generate(1, 1);
	REQUIRE(single.size() == 1);
	REQUIRE(DistinctNonzeroQuestIds(single).size() == 1);
}

TEST_CASE("questCount zero is treated as a single quest")
{
	const auto batch = F4DH::Core::StressTestDataGenerator::Generate(27, 0);
	const auto ids = DistinctNonzeroQuestIds(batch);
	REQUIRE(ids.size() == 1);
	REQUIRE(*ids.begin() == F4DH::Core::StressTestDataGenerator::kSyntheticQuestIdBase);
}

TEST_CASE("every nonzero synthetic questId is at or above the reserved base")
{
	const auto batch = F4DH::Core::StressTestDataGenerator::Generate(500, 20);
	for (const auto& line : batch) {
		if (line.questId != 0) {
			REQUIRE(line.questId >= F4DH::Core::StressTestDataGenerator::kSyntheticQuestIdBase);
		}
	}
}

TEST_CASE("batch populates every view bucket and includes unattributed lines")
{
	// Bucket map cross-check — PrismaUI_F4/views/DialogueHistory/history.js
	// dhBucketFor (C7, frozen in GATES.md): raw 1-5 -> "main", 6 -> "misc",
	// >=7 -> "side" (DLC types land here), 0/invalid -> "unattributed".
	const auto batch = F4DH::Core::StressTestDataGenerator::Generate(500, 20);

	bool main = false;
	bool misc = false;
	bool side = false;
	bool unattributed = false;
	for (const auto& line : batch) {
		if (line.questId == 0) {
			REQUIRE(line.questType == 0);
			REQUIRE(line.questName.empty());
			unattributed = true;
			continue;
		}
		if (line.questType >= 1 && line.questType <= 5) {
			main = true;
		} else if (line.questType == 6) {
			misc = true;
		} else if (line.questType >= 7) {
			side = true;
		}
	}
	REQUIRE(main);
	REQUIRE(misc);
	REQUIRE(side);
	REQUIRE(unattributed);
}

TEST_CASE("batch contains both player and npc speaker kinds")
{
	const auto batch = F4DH::Core::StressTestDataGenerator::Generate(40, 4);

	bool player = false;
	bool npc = false;
	for (const auto& line : batch) {
		if (line.kind == F4DH::Core::SpeakerKind::Player) {
			REQUIRE(line.speaker == "Player");
			player = true;
		} else if (line.kind == F4DH::Core::SpeakerKind::Npc) {
			REQUIRE(!line.speaker.empty());
			npc = true;
		}
	}
	REQUIRE(player);
	REQUIRE(npc);
}

TEST_CASE("generated text is non-empty with at least three distinct lengths")
{
	const auto batch = F4DH::Core::StressTestDataGenerator::Generate(40, 4);

	std::set<std::size_t> lengths;
	for (const auto& line : batch) {
		REQUIRE(!line.text.empty());
		lengths.insert(line.text.size());
	}
	REQUIRE(lengths.size() >= 3);
}

TEST_CASE("batch serializes through BuildSnapshot with a leading bracket and every questId")
{
	const auto batch = F4DH::Core::StressTestDataGenerator::Generate(500, 20);
	const std::string snapshot = F4DH::Core::PayloadBuilder::BuildSnapshot(batch);

	REQUIRE(!snapshot.empty());
	REQUIRE(snapshot.front() == '[');

	const auto ids = DistinctNonzeroQuestIds(batch);
	REQUIRE(ids.size() == 20);
	for (const std::uint32_t id : ids) {
		REQUIRE(snapshot.find(std::to_string(id)) != std::string::npos);
	}
}

TEST_CASE("generate is deterministic for identical inputs")
{
	const auto first = F4DH::Core::StressTestDataGenerator::Generate(50, 5);
	const auto second = F4DH::Core::StressTestDataGenerator::Generate(50, 5);

	REQUIRE(first.size() == second.size());
	for (std::size_t i = 0; i < first.size(); ++i) {
		REQUIRE(first[i].speaker == second[i].speaker);
		REQUIRE(first[i].kind == second[i].kind);
		REQUIRE(first[i].text == second[i].text);
		REQUIRE(first[i].questId == second[i].questId);
		REQUIRE(first[i].questName == second[i].questName);
		REQUIRE(first[i].questType == second[i].questType);
	}
}
