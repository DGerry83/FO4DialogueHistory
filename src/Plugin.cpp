#include "PCH.h"

#include "Composition.h"

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
// so one binary serves OG/NG/AE.
F4SE_PLUGIN_VERSION = []() noexcept {
	F4SE::PluginVersionData v{};
	v.SetPluginVersion(REX::Version{ F4DH_VERSION_MAJOR, F4DH_VERSION_MINOR, F4DH_VERSION_PATCH, 0 });
	v.SetPluginName("FO4DialogueHistory");
	v.SetUseAddressLibrary(true);
	v.SetIsLayoutDependent(true);
	v.SetCompatibleVersions({ F4SE::RUNTIME_LATEST });
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

	const auto messaging = F4SE::GetMessagingInterface();
	messaging->RegisterListener(OnF4SEMessage);

	REX::LogInformation("{} v{} loaded", F4SE::GetPluginName(), F4SE::GetPluginVersion());
	return true;
}
