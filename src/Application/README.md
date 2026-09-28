# Application layer

Use cases and orchestration. Depends on Core only; touches Infrastructure exclusively through the interfaces defined here (`ISubtitleSink`, `IViewBridge`, `ISettingsStore`).

Belongs here: `CapturePipeline` (filter → buffer → view routing), `ViewController` (Hidden/Open/Degraded state machine), `HotkeyController` (key → toggle), `StressTestInjector` (INI-gated synthetic batch → buffer), `Settings` value object, the `NullViewBridge` degraded-mode fallback.

Does not belong: `RE::` types, PrismaUI API calls, INI parsing, hook trampolines.
