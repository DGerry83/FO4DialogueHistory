#pragma once

namespace F4DH
{
	// Composition root — the ONLY place concrete Infrastructure components
	// are instantiated and wired into Application components.
	namespace Composition
	{
		// Called from F4SE messaging (kPostLoad / kGameDataReady phases).
		// Returns false on fatal init failure (hook install, version check).
		bool InitializePostLoad();
		bool InitializeDataLoaded();
	}
}
