#pragma once

#include <algorithm>
#include <cstddef>
#include <deque>
#include <mutex>
#include <vector>

#include "MessageLine.h"

namespace F4DH::Core
{
	// FIFO buffer of the most recent accepted-for-display HUD messages.
	// Unlike DialogueBuffer there is no unlimited mode: the construction
	// capacity is clamped to [kMinCapacity, kMaxCapacity] and the oldest
	// message is evicted in O(1) once full. Thread-safe: capture and panel
	// paths may run on different threads — all access under _mutex.
	class MessageBuffer
	{
	public:
		static constexpr std::size_t kMinCapacity = 1;
		static constexpr std::size_t kMaxCapacity = 1000;
		static constexpr std::size_t kDefaultCapacity = 200;

		explicit MessageBuffer(std::size_t a_capacity = kDefaultCapacity) :
			_capacity(std::clamp(a_capacity, kMinCapacity, kMaxCapacity))
		{}

		void Push(MessageLine a_message)
		{
			const std::lock_guard lock(_mutex);
			if (_messages.size() >= _capacity) {
				_messages.pop_front();
			}
			_messages.push_back(std::move(a_message));
		}

		void Clear()
		{
			const std::lock_guard lock(_mutex);
			_messages.clear();
		}

		[[nodiscard]] std::size_t Size() const
		{
			const std::lock_guard lock(_mutex);
			return _messages.size();
		}
		[[nodiscard]] std::size_t Capacity() const { return _capacity; }

		// Oldest-first snapshot.
		[[nodiscard]] std::vector<MessageLine> Snapshot() const
		{
			const std::lock_guard lock(_mutex);
			return { _messages.begin(), _messages.end() };
		}

	private:
		std::size_t             _capacity;
		std::deque<MessageLine> _messages;
		mutable std::mutex      _mutex;
	};
}
