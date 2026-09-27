# Infrastructure layer

File I/O, engine hooks, external APIs, UI framework adapters, DI wiring (`src/Composition.cpp`). Depends on Core + Application.

Belongs here: `SubtitleHook` (the `SubtitleManager::ShowSubtitle` detour and RE→`SubtitleEvent` translation), `PrismaViewBridge` (PrismaUI F4 runtime API), `IniSettingsStore`, `Log`, the vendored `PrismaUI_F4_API.h`.

Rules:
- Engine types (`RE::`, `F4SE::`) must not leak past this layer — translate to `Core` value types at the boundary.
- Hook-path code: O(1), no disk I/O, no unbounded allocation (see PLANNING_WORKSHEET native-interop checklist).
- PrismaUI is resolved at runtime (`GetProcAddress`), never linked.
