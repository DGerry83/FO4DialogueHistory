# Core.Tests

Catch2 v3 unit tests for `src/Core/` (wired in CMake at milestone 2).

Planned suites (see IMPLEMENTATION_PLAN.md §9, milestone 2):

- `DialogueBufferTests.cpp` — capacity eviction, FIFO order, snapshot, clear, zero-capacity guard.
- `ConversationFilterTests.cpp` — truth table over `SubtitleEvent` flags (menuOpen / sceneIsPlayerDialogue / speakerIsPlayer+spokenToPlayer / all-false bark rejection).
- `PayloadBuilderTests.cpp` — snapshot array shape, append object shape, JSON escaping of quotes/backslashes/control chars.

These tests run outside the game (plain C++). In-game verification lives in `docs/Testing.md`.
