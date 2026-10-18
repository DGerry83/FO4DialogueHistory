#include "PCH.h"

#include "Composition.h"

#include "Infrastructure/Cosave/CosaveStore.h"

namespace
{
	void OnF4SEMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->GetType()) {
		case F4SE::MessagingInterface::MessageType::kPostLoad:
			F4DH::Composition::InitializePostLoad();
			break;
		case F4SE::MessagingInterface::MessageType::kGameDataReady:
			F4DH::Composition::InitializeDataLoaded();
			break;
		default:
			break;
		}
	}
}

// F4SEPlugin_Version data export. The AV fork resolves this symbol
// internally (PluginVersionData::GetSingleton) and NG/AE F4SE reads it;
// OG 1.10.163 ignores it and uses the Query/Load pair below. Address Library
// so one binary serves OG/NG/AE. The per-runtime bits must BOTH be set: the
// generic SetUseAddressLibrary/SetIsLayoutDependent aliases write only the AE
// bits (1<<2), and NG F4SE (1.10.980/984) rejects the plugin unless it sees
// its own bits (1<<1) or its runtime in the compatible list (2026-10-18).
F4SE_PLUGIN_VERSION = []() noexcept {
	F4SE::PluginVersionData v{};
	v.SetPluginVersion(REX::Version{ F4DH_VERSION_MAJOR, F4DH_VERSION_MINOR, F4DH_VERSION_PATCH, 0 });
	v.SetPluginName("FO4DialogueHistory");
	v.SetUseAddressLibrary_RuntimeNG(true);
	v.SetUseAddressLibrary_RuntimeAE(true);
	v.SetIsLayoutDependent_RuntimeNG(true);
	v.SetIsLayoutDependent_RuntimeAE(true);
	v.SetCompatibleVersions({ F4SE::RUNTIME_LATEST_NG, F4SE::RUNTIME_LATEST_AE });
	return v;
}();

// OG entry point (F4SE 0.6.x). Without this export the loader rejects the
// DLL outright ("does not appear to be an F4SE plugin").
F4SE_PLUGIN_QUERY(const F4SE::QueryInterface*, F4SE::PluginInfo* a_info)
{
	const auto data = F4SE::PluginVersionData::GetSingleton();
	a_info->SetDataVersion(F4SE::PluginInfo::DATA_VERSION);
	a_info->SetPluginName(data->GetPluginName());
	a_info->SetPluginVersion(data->GetPluginVersion());

	return true;
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se, { .logName = "FO4DialogueHistory.log" });

	// Environment banner first: every field log must identify the exact
	// runtime/F4SE/plugin build it came from (D1, 2026-10-18).
	constexpr std::string_view kRuntimeNames[] = { "none", "VR", "OG", "NG", "AE" };
	REX::LogInformation("environment: game {} ({}), F4SE {}, plugin v{}",
		F4SE::GetRuntimeVersion(),
		kRuntimeNames[std::min(static_cast<std::size_t>(F4SE::GetRuntimeType()), std::size(kRuntimeNames) - 1)],
		F4SE::GetF4SEVersion(), F4SE::GetPluginVersion());

	const auto messaging = F4SE::GetMessagingInterface();
	messaging->RegisterListener(OnF4SEMessage);

	// Cosave registration must happen at plugin load (the interface resolves
	// the plugin handle internally); the buffer is bound later, at
	// game-data-ready — the callbacks null-guard until then.
	F4DH::Infrastructure::CosaveStore::Install();

	REX::LogInformation("{} v{} loaded", F4SE::GetPluginName(), F4SE::GetPluginVersion());
	return true;
}
