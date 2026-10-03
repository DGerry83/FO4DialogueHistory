# Core layer

Domain logic, entities, state, and rules. **Zero external dependencies** — no F4SE, no CommonLibF4 `RE::` types, no PrismaUI, no file I/O. Everything here must compile as plain C++20 and be unit-testable (see `tests/Core.Tests/`).

Belongs here: `DialogueLine`, `SubtitleEvent`, `DialogueBuffer`, `ConversationFilter`, `PayloadBuilder`, `ViewFilterState`, `ILogger` (abstraction only).

Does not belong: engine reads, UI calls, settings file access — those are Infrastructure, reached through interfaces defined in Application (`IViewBridge`, `ISettingsStore`, `ISubtitleSink`).
