#include "PrismaViewBridge.h"

#include <functional>
#include <utility>

#include "REX/LOG.h"

// <Windows.h> (pulled in by PrismaUI_F4_API.h) defines an ERROR macro that
// collides with REX::ERROR.
#ifdef ERROR
#	undef ERROR
#endif

// PrismaUI F4 bridge (runtime-resolved via RequestPluginAPI — never linked).
// Threading: JS listener callbacks and any off-game-thread caller are
// marshaled to the game thread via IVPrismaUI11::DispatchToGameThread, so
// all bridge internals run exclusively on the game thread.
// Data flow: PushSnapshot caches the payload and forwards it; the view's
// requestHistory listener (fired on DOM ready) replays the cache, because
// PrismaUI must not receive InteropCall before the DOM is ready.

namespace F4DH::Infrastructure
{
	namespace
	{
		constexpr auto kViewPath{ "PrismaUI_F4/views/DialogueHistory/index.html" };

		// The bridge is a composition-owned singleton. These mirrors let the
		// plain-function callbacks reach it without header changes.
		PrismaViewBridge*                g_bridge{ nullptr };
		PRISMA_UI_API::IVPrismaUI11*     g_api{ nullptr };
		PrismaViewBridge::CloseCallback  g_closeCallback{ nullptr };
		std::string                      g_snapshotCache;
		bool                             g_prismaMissingLogged{ false };

		void InvokeTask(void* a_userdata)
		{
			auto* task = static_cast<std::function<void()>*>(a_userdata);
			(*task)();
			delete task;
		}

		// Marshals a task to the game thread; drops and logs on failure.
		void Dispatch(std::function<void()> a_task)
		{
			if (!g_api) {
				return;
			}
			auto* heap = new std::function<void()>(std::move(a_task));
			if (!g_api->DispatchToGameThread(&InvokeTask, heap)) {
				REX::ERROR("PrismaViewBridge: DispatchToGameThread failed; operation dropped");
				delete heap;
			}
		}

		void OnRequestHistory(const char*)
		{
			// Fires on PrismaUI's thread; replay on the game thread.
			// PushSnapshot re-caches (same value) and health-checks internally.
			const std::string cached = g_snapshotCache;
			Dispatch([cached] {
				if (g_bridge) {
					g_bridge->PushSnapshot(cached);
				}
			});
		}

		void OnCloseRequested(const char*)
		{
			// Fires on PrismaUI's thread; route on the game thread.
			Dispatch([] {
				if (g_closeCallback) {
					g_closeCallback();
				}
			});
		}

		void OnDomReady(PrismaView a_view)
		{
			if (!g_api) {
				return;
			}
			g_api->RegisterJSListener(a_view, "requestHistory", &OnRequestHistory);
			g_api->RegisterJSListener(a_view, "closeRequested", &OnCloseRequested);
			g_api->SetViewRole(a_view, PRISMA_UI_API::ViewRole::kPanel);
			g_api->SetViewOwnsEscape(a_view, true);
		}
	}

	PrismaViewBridge::PrismaViewBridge() :
		_api(PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI11>())
	{
		g_bridge = this;
		g_api = _api;
	}

	bool PrismaViewBridge::Create()
	{
		if (!_api) {
			if (!g_prismaMissingLogged) {
				g_prismaMissingLogged = true;
				REX::ERROR("FO4DialogueHistory: PrismaUI F4 not found — UI disabled, capture continuing.");
			}
			return false;
		}

		if (!_api->IsGameThread()) {
			Dispatch([this] { Create(); });
			return true;  // optimistic; queued work runs in order on the game thread
		}

		// Dead-view recreation: destroy any previous handle first so no path
		// leaks a PrismaView.
		if (_view != 0) {
			_api->Destroy(_view);
			_view = 0;
		}

		_view = _api->CreateView(kViewPath, &OnDomReady);
		return _view != 0;
	}

	void PrismaViewBridge::Show()
	{
		if (!_api || _view == 0) {
			return;
		}
		if (!_api->IsGameThread()) {
			Dispatch([this] { Show(); });
			return;
		}
		_api->Show(_view);
	}

	void PrismaViewBridge::Hide()
	{
		if (!_api || _view == 0) {
			return;
		}
		if (!_api->IsGameThread()) {
			Dispatch([this] { Hide(); });
			return;
		}
		_api->Hide(_view);
	}

	void PrismaViewBridge::Focus()
	{
		if (!_api || _view == 0) {
			return;
		}
		if (!_api->IsGameThread()) {
			Dispatch([this] { Focus(); });
			return;
		}
		_api->Focus(_view, false);  // never pause the game
	}

	void PrismaViewBridge::Unfocus()
	{
		if (!_api || _view == 0) {
			return;
		}
		if (!_api->IsGameThread()) {
			Dispatch([this] { Unfocus(); });
			return;
		}
		_api->Unfocus(_view);
	}

	void PrismaViewBridge::PushSnapshot(const std::string& json)
	{
		if (!_api || _view == 0) {
			return;
		}
		if (!_api->IsGameThread()) {
			Dispatch([this, json] { PushSnapshot(json); });
			return;
		}
		g_snapshotCache = json;  // replayed by the view's requestHistory listener
		if (IsHealthy()) {
			_api->InteropCall(_view, "setHistory", json.c_str());
		}
	}

	void PrismaViewBridge::AppendLine(const std::string& json)
	{
		if (!_api || _view == 0) {
			return;
		}
		if (!_api->IsGameThread()) {
			Dispatch([this, json] { AppendLine(json); });
			return;
		}
		if (IsHealthy()) {
			_api->InteropCall(_view, "appendLine", json.c_str());
		}
	}

	void PrismaViewBridge::SetCloseCallback(CloseCallback fn)
	{
		_closeCallback = fn;
		g_closeCallback = fn;
	}

	bool PrismaViewBridge::IsHealthy()
	{
		if (!_api || _view == 0) {
			return false;
		}
		if (!_api->IsValid(_view)) {
			return false;
		}
		const auto health = _api->GetViewHealth(_view);
		return health == PRISMA_UI_API::ViewHealth::kLive ||
		       health == PRISMA_UI_API::ViewHealth::kDomReady;
	}
}
