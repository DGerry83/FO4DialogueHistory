#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

#include "DialogueLine.h"

namespace F4DH::Core
{
	// FIFO buffer of the most recent dialogue lines. Capacity 0 = unlimited
	// (no eviction); >0 retains only the newest `capacity` lines.
	// Thread-safe: the subtitle hook pushes from multiple engine threads
	// (U1b runtime evidence, 2026-09-28) while the hotkey path snapshots on a
	// UIJob worker thread (2026-09-27 crash fix) — all access under _mutex.
	class DialogueBuffer
	{
	public:
		explicit DialogueBuffer(std::size_t capacity = 50) :
			_capacity(capacity)
		{}

		void Push(DialogueLine line)
		{
			const std::lock_guard lock(_mutex);
			if (_capacity > 0 && _lines.size() >= _capacity) {
				_lines.pop_front();
			}
			_lines.push_back(std::move(line));
		}

		void Clear()
		{
			const std::lock_guard lock(_mutex);
			_lines.clear();
		}

		// Erases every line owned by the given quest (0 = unattributed).
		// Returns the number of removed lines; the relative order of the
		// surviving lines is preserved.
		[[nodiscard]] std::size_t RemoveQuest(std::uint32_t questId)
		{
			const std::lock_guard lock(_mutex);
			const auto            before = _lines.size();
			std::erase_if(_lines, [questId](const DialogueLine& line) { return line.questId == questId; });
			return before - _lines.size();
		}

		[[nodiscard]] std::size_t Size() const
		{
			const std::lock_guard lock(_mutex);
			return _lines.size();
		}
		[[nodiscard]] std::size_t Capacity() const { return _capacity; }

		// Oldest-first snapshot.
		[[nodiscard]] std::vector<DialogueLine> Snapshot() const
		{
			const std::lock_guard lock(_mutex);
			return { _lines.begin(), _lines.end() };
		}

	private:
		std::size_t              _capacity;
		std::deque<DialogueLine> _lines;
		mutable std::mutex       _mutex;
	};
}
