#include "PCH.h"

#include "Composition.h"
#include "Infrastructure/Logging/Log.h"

// F4SE plugin entry point. Metadata uses the Address Library so one binary
// serves 1.10.163 and next-gen runtimes.
//
// Milestone 1 completion checklist:
// - F4SEPluginVersionData: pluginVersion from project(), author, no
//   version dependence (UsesAddressLibrary(true), IsLayoutDependent(true)).
// - F4SEPluginLoad: Log::Init, register messaging listener dispatching
//   kPostLoad -> Composition::InitializePostLoad and
//   kDataLoaded -> Composition::InitializeDataLoaded.
//
// extern "C" __declspec(dllexport) constinit F4SE::PluginVersionData F4SEPlugin_Version = ...;
// extern "C" __declspec(dllexport) bool F4SEPlugin_Load(const F4SE::LoadInterface* f4se) { ... }
