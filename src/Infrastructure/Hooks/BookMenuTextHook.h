#pragma once

namespace F4DH::Core
{
	class ILogger;
}

namespace F4DH::Application
{
	class INoteSink;
}

namespace F4DH::Infrastructure
{
	// M1 follow-up diagnostic (NEXT_PROBE_CONTRACT.md): OG-only prologue
	// detour on BookMenu::PrepareBookText that captures the prepared display
	// text after the original runs. Answers whether books reaching BookMenu
	// get native text preparation (Boston case) and whether the post-original
	// buffer carries final alias-resolved text (G2). Installed ONLY on OG
	// (F4SE::IsRuntimeOnlyOG) — on NG/AE Install logs that the detour is
	// disabled and resolves no Address Library IDs. The live 9-byte entry is
	// validated against the contract sequence before Enable; mismatch or any
	// soft failure degrades cleanly (never fatal to dialogue/message capture).
	// Successful detours are process-lifetime; the 33-byte continuation is
	// armed before Enable so no intercepted call can route into null bytes.
	namespace BookMenuTextHook
	{
		// Returns false only on a soft install failure (live-entry mismatch or
		// enable refusal); non-OG runtimes resolve here as disabled and return
		// true. Address Library / WriteSafe failures are fork-hard (REX::Fail)
		// and are not claimed recoverable.
		bool Install(Core::ILogger& a_log, bool a_verbose, Application::INoteSink* a_sink = nullptr);
	}
}
