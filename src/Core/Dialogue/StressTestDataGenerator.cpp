#include "StressTestDataGenerator.h"

#include <string>

namespace
{
	// Quest-type rotation — quest i gets kQuestTypeRotation[i % 10]. Frozen
	// against the view's bucket map in PrismaUI_F4/views/DialogueHistory/
	// history.js (dhBucketFor, C7): raw 1-5 -> "main", 6 -> "misc", 7 -> "side",
	// >=8 -> "side" (DLC), 0/invalid -> "unattributed". The first ten quests
	// cover every bucket the view can render.
	constexpr std::uint8_t kQuestTypeRotation[10] = { 1, 7, 6, 2, 10, 12, 3, 4, 5, 8 };

	// Display label per quest type — same names as the view's dhTypeNames,
	// bucket-obvious and deterministic.
	const char* QuestTypeLabel(std::uint8_t a_questType)
	{
		switch (a_questType) {
		case 1:
		case 2:
		case 3:
		case 4:
		case 5: return "Main";
		case 6: return "Misc";
		case 7: return "Side";
		case 10: return "FarHarbor";
		case 11: return "NukaWorld";
		case 12: return "Contraptions";
		default: return "DLC";  // 8, 9, 13, anything else >= 8
		}
	}

	std::string IndexTag(std::uint32_t a_lineIndex)
	{
		std::string tag = "#";
		const std::string digits = std::to_string(a_lineIndex);
		if (digits.size() < 4) {
			tag.append(4 - digits.size(), '0');
		}
		tag += digits;
		return tag;
	}
}

namespace F4DH::Core
{
	std::vector<DialogueLine> StressTestDataGenerator::Generate(std::uint32_t lineCount, std::uint32_t questCount)
	{
		const std::uint32_t quests = questCount == 0 ? 1 : questCount;
		std::vector<DialogueLine> lines;
		lines.reserve(lineCount);

		std::uint32_t attributedIndex = 0;  // round-robin counter over attributed lines only
		for (std::uint32_t lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
			DialogueLine line;
			if (lineIndex % 10 == 9) {
				// Unattributed — quest fields stay at their defaults (0/""/0);
				// overrides the round-robin without consuming one of its slots.
			} else {
				const std::uint32_t questIndex = attributedIndex % quests;
				++attributedIndex;
				line.questId = kSyntheticQuestIdBase + questIndex;
				line.questName = MakeQuestName(questIndex);
				line.questType = kQuestTypeRotation[questIndex % 10];
			}

			if (lineIndex % 4 == 3) {
				line.kind = SpeakerKind::Player;
				line.speaker = "Player";
			} else {
				line.kind = SpeakerKind::Npc;
				line.speaker = MakeSpeaker(lineIndex);
			}

			line.text = MakeText(lineIndex);
			lines.push_back(std::move(line));
		}
		return lines;
	}

	std::string StressTestDataGenerator::MakeQuestName(std::uint32_t questIndex)
	{
		std::string name = "Stress ";
		name += QuestTypeLabel(kQuestTypeRotation[questIndex % 10]);
		name += ' ';
		const std::uint32_t number = questIndex + 1;  // 1-based, zero-padded to 2
		if (number < 10) {
			name += '0';
		}
		name += std::to_string(number);
		return name;
	}

	std::string StressTestDataGenerator::MakeSpeaker(std::uint32_t lineIndex)
	{
		const std::uint32_t npc = (lineIndex % 12) + 1;  // "Stress NPC 01" .. "Stress NPC 12"
		std::string speaker = "Stress NPC ";
		if (npc < 10) {
			speaker += '0';
		}
		speaker += std::to_string(npc);
		return speaker;
	}

	std::string StressTestDataGenerator::MakeText(std::uint32_t lineIndex)
	{
		static constexpr const char* kMedium =
			" Checkpoint is quiet, but the readings say otherwise. "
			"The squad pushes on through the ruins toward the marker, weapons up.";
		static constexpr const char* kLongExtra =
			" Every door is a maybe, and the radio keeps hissing static that almost "
			"sounds like a voice counting down. Nobody says the word ambush, because "
			"saying it makes it true.";

		std::string text = IndexTag(lineIndex);
		switch (lineIndex % 3) {
		case 0:
			text += " Move up. Stay low.";  // short bark (~24 chars with tag)
			break;
		case 1:
			text += kMedium;  // ~130 chars
			break;
		default:
			text += kMedium;   // ~290 chars total
			text += kLongExtra;
			break;
		}
		return text;
	}
}
