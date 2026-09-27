#include "PrismaViewBridge.h"

// Milestone 4: implement against IVPrismaUI11.
// - Create: CreateView("PrismaUI_F4/views/DialogueHistory/index.html", onDomReady);
//   RegisterJSListener("requestHistory") + RegisterJSListener("closeRequested");
//   SetViewRole(kPanel); SetViewOwnsEscape(true).
// - PushSnapshot/AppendLine: InteropCall / Invoke with the JSON payload.
// - Focus: Focus(view, /*pauseGame=*/false); Unfocus on close.
// - IsHealthy: IsValid && GetViewHealth(view) == kLive (API V8+).
// - Threading: marshal pushes with DispatchToGameThread if not IsGameThread.

namespace F4DH::Infrastructure
{
	PrismaViewBridge::PrismaViewBridge() :
		_api(PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI11>())
	{}

	bool PrismaViewBridge::Create()
	{
		return false;  // stub — milestone 4
	}

	void PrismaViewBridge::Show() {}
	void PrismaViewBridge::Hide() {}
	void PrismaViewBridge::Focus() {}
	void PrismaViewBridge::Unfocus() {}
	void PrismaViewBridge::PushSnapshot(const std::string&) {}
	void PrismaViewBridge::AppendLine(const std::string&) {}
	void PrismaViewBridge::SetCloseCallback(CloseCallback fn) { _closeCallback = fn; }

	bool PrismaViewBridge::IsHealthy()
	{
		return _api != nullptr && _view != 0;
	}
}
