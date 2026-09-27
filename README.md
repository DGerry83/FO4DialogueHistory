# FO4 Dialogue History

A Fallout 4 mod that lets the player re-read recent conversation dialogue. An F4SE C++ plugin captures subtitle lines from player-involved conversation scenes (speaker + text), keeps the last 50 lines, and shows them in a PrismaUI F4 panel opened via hotkey — a scrollable log, newest at the bottom.

**Status:** greenfield — design spec complete, no code yet. See `.flybywire/active/2026-09-27_DesignSpec_DialogueHistory/DESIGN_SPEC.md`.

## Requirements

- Fallout 4 (1.10.163 or next-gen 1.10.984+)
- F4SE
- Address Library for F4SE
- PrismaUI F4 ≥ 2.2.1.1

## Repository layout

- `src/` — F4SE C++ plugin source (to be created)
- `PrismaUI_F4/views/DialogueHistory/` — HTML/JS/CSS view assets (deploy under `Data/`)
- `F4SE/Plugins/` — INI settings (deploy under `Data/`)
- `docs/` — build/test/recipe guides for agents
- `.flybywire/` — workflow session artifacts (FlyByWire)
- `AGENTS.md` — agent onboarding (start there)
