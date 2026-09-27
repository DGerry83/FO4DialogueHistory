#pragma once

#include <cstddef>
#include <deque>
#include <vector>

#include "DialogueLine.h"

namespace F4DH::Core
{
	// Fixed-capacity FIFO ring of the most recent dialogue lines.
	class DialogueBuffer
	{
	public:
		explicit DialogueBuffer(std::size_t capacity = 50) :
			_capacity(capacity)
		{}

		void Push(DialogueLine line)
		{
			if (_capacity == 0) {
				return;
			}
			if (_lines.size() >= _capacity) {
				_lines.pop_front();
			}
			_lines.push_back(std::move(line));
		}

		void Clear() { _lines.clear(); }

		[[nodiscard]] std::size_t Size() const { return _lines.size(); }
		[[nodiscard]] std::size_t Capacity() const { return _capacity; }

		// Oldest-first snapshot.
		[[nodiscard]] std::vector<DialogueLine> Snapshot() const
		{
			return { _lines.begin(), _lines.end() };
		}

	private:
		std::size_t             _capacity;
		std::deque<DialogueLine> _lines;
	};
}
