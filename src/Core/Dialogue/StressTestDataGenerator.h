#pragma once

#include <cstdint>
#include <vector>

#include "DialogueLine.h"

namespace F4DH::Core
{
	// Deterministic synthetic dialogue generator for stress-testing the view
	// render path. Produces existing DialogueLine shapes only — engine-free,
	// index-driven, no RNG, no I/O — so it is unit-testable and safe to call
	// from the input thread. Synthetic questIds come from the reserved
	// 0xFF000000+ range: the FF load-order byte is runtime-reserved, so no
	// ESM/ESP/ESL record can occupy it and clear-per-quest on an injected
	// entry can never touch a real quest.
	class StressTestDataGenerator
	{
	public:
		static constexpr std::uint32_t kSyntheticQuestIdBase = 0xFF000000;

		// Exactly lineCount lines. questCount 0 is treated as 1. Every 10th
		// line (index % 10 == 9) is unattributed and does not consume a
		// round-robin slot; the remaining lines round-robin across the
		// questCount synthetic quests (quest i -> kSyntheticQuestIdBase + i).
		[[nodiscard]] static std::vector<DialogueLine> Generate(std::uint32_t lineCount, std::uint32_t questCount);

	private:
		[[nodiscard]] static std::string MakeQuestName(std::uint32_t questIndex);
		[[nodiscard]] static std::string MakeSpeaker(std::uint32_t lineIndex);
		[[nodiscard]] static std::string MakeText(std::uint32_t lineIndex);
	};
}
