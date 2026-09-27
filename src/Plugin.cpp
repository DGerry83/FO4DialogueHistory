#include "PCH.h"

#include "Composition.h"

namespace
{
	void OnF4SEMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case F4SE::MessagingInterface::kPostLoad:
			F4DH::Composition::InitializePostLoad();
			break;
		case F4SE::MessagingInterface::kGameDataReady:
			F4DH::Composition::InitializeDataLoaded();
			break;
		default:
			break;
		}
	}
}

// F4SE plugin entry point. Metadata uses the Address Library so one binary
// serves 1.10.163 and next-gen runtimes.
F4SE_PLUGIN_VERSION = []() noexcept {
	F4SE::PluginVersionData v{};
	v.PluginVersion({ F4DH_VERSION_MAJOR, F4DH_VERSION_MINOR, F4DH_VERSION_PATCH, 0 });
	v.PluginName("FO4DialogueHistory");
	v.UsesAddressLibrary(true);
	v.IsLayoutDependent(true);
	v.CompatibleVersions({ F4SE::RUNTIME_LATEST });
	return v;
}();

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se, { .logName = "FO4DialogueHistory.log" });

	const auto messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(OnF4SEMessage)) {
		REX::ERROR("failed to register F4SE messaging listener");
		return false;
	}

	REX::INFO("{} v{} loaded", F4SE::GetPluginName(), F4SE::GetPluginVersion());
	return true;
}
