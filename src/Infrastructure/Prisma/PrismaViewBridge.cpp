#include "PrismaViewBridge.h"

#include <functional>
#include <utility>

#include "REX/Log.hpp"

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
		// Resolved by PrismaUI as Data/PrismaUI_F4/views/<path> — the
		// PrismaUI_F4/views prefix must NOT be repeated here.
		constexpr auto kViewPath{ "DialogueHistory/index.html" };

		// The bridge is a composition-owned singleton. These mirrors let the
		// plain-function callbacks reach it without header changes.
		PrismaViewBridge*                g_bridge{ nullptr };
		PRISMA_UI_API::IVPrismaUI11*     g_api{ nullptr };
		PrismaViewBridge::CloseCallback  g_closeCallback{ nullptr };
		std::string                      g_snapshotCache;
		bool                             g_prismaMissingLogged{ false };
		bool                             g_createResultLogged{ false };
		// Set by Focus(), cleared by Unfocus()/Hide(); applied in OnDomReady
		// because PrismaUI silently drops Focus before the DOM is ready.
		bool                             g_focusPending{ false };
		// Set by Show() when the view does not exist yet (first open races the
		// async create); applied by Create() right after CreateView succeeds.
		bool                             g_showPending{ false };

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
				REX::LogError("PrismaViewBridge: DispatchToGameThread failed; operation dropped");
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

		void OnConsoleMessage(PrismaView, PRISMA_UI_API::ConsoleMessageLevel a_level, const char* a_message)
		{
			switch (a_level) {
			case PRISMA_UI_API::ConsoleMessageLevel::Error:
				REX::LogError("[JS] {}", a_message);
				break;
			case PRISMA_UI_API::ConsoleMessageLevel::Warning:
				REX::LogWarning("[JS] {}", a_message);
				break;
			default:
				REX::LogInformation("[JS] {}", a_message);
				break;
			}
		}

		void OnDomReady(PrismaView a_view)
		{
			if (!g_api) {
				return;
			}
			g_api->RegisterConsoleCallback(a_view, &OnConsoleMessage);
			g_api->RegisterJSListener(a_view, "requestHistory", &OnRequestHistory);
			g_api->RegisterJSListener(a_view, "closeRequested", &OnCloseRequested);
			g_api->SetViewRole(a_view, PRISMA_UI_API::ViewRole::kPanel);
			g_api->SetViewOwnsEscape(a_view, true);
			// Deterministic snapshot replay: the view's requestHistory can fire
			// before the listener is registered, so push the cache directly.
			if (g_bridge && !g_snapshotCache.empty()) {
				g_bridge->PushSnapshot(g_snapshotCache);
			}
			if (g_focusPending) {
				g_focusPending = false;
				g_api->Focus(a_view, false);
			}
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
				REX::LogError("FO4DialogueHistory: PrismaUI F4 not found — UI disabled, capture continuing.");
			}
			return false;
		}

		if (!_api->IsGameThread()) {
			Dispatch([this] { static_cast<void>(Create()); });
			return true;  // optimistic; queued work runs in order on the game thread
		}

		// Dead-view recreation: destroy any previous handle first so no path
		// leaks a PrismaView.
		if (_view != 0) {
			_api->Destroy(_view);
			_view = 0;
		}

		_view = _api->CreateView(kViewPath, &OnDomReady);
		if (!g_createResultLogged) {
			g_createResultLogged = true;
			if (_view != 0 && _api->IsValid(_view)) {
				REX::LogInformation("PrismaViewBridge: CreateView ok ('{}'), IsValid=true", kViewPath);
			} else {
				REX::LogError("PrismaViewBridge: CreateView failed or view invalid ('{}')", kViewPath);
			}
		}
		if (_view != 0 && g_showPending) {
			g_showPending = false;
			_api->Show(_view);
			REX::LogInformation("PrismaViewBridge: panel shown");
		}
		return _view != 0;
	}

	void PrismaViewBridge::Show()
	{
		if (!_api) {
			return;
		}
		if (!_api->IsGameThread()) {
			Dispatch([this] { Show(); });
			return;
		}
		if (_view == 0) {
			g_showPending = true;  // applied by Create() once the view exists
			return;
		}
		_api->Show(_view);
		REX::LogInformation("PrismaViewBridge: panel shown");
	}

	void PrismaViewBridge::Hide()
	{
		if (!_api) {
			return;
		}
		if (!_api->IsGameThread()) {
			Dispatch([this] { Hide(); });
			return;
		}
		g_showPending = false;
		g_focusPending = false;
		if (_view == 0) {
			return;
		}
		_api->Hide(_view);
		REX::LogInformation("PrismaViewBridge: panel hidden");
	}

	void PrismaViewBridge::Focus()
	{
		if (!_api) {
			return;
		}
		if (!_api->IsGameThread()) {
			Dispatch([this] { Focus(); });
			return;
		}
		g_focusPending = true;  // re-applied by OnDomReady if the DOM is not ready yet
		if (_view == 0) {
			return;  // applied after Create() via OnDomReady
		}
		_api->Focus(_view, false);  // never pause the game
	}

	void PrismaViewBridge::Unfocus()
	{
		if (!_api) {
			return;
		}
		if (!_api->IsGameThread()) {
			Dispatch([this] { Unfocus(); });
			return;
		}
		g_focusPending = false;
		if (_view == 0) {
			return;
		}
		_api->Unfocus(_view);
	}

	void PrismaViewBridge::PushSnapshot(const std::string& json)
	{
		if (!_api) {
			return;
		}
		if (!_api->IsGameThread()) {
			Dispatch([this, json] { PushSnapshot(json); });
			return;
		}
		g_snapshotCache = json;  // replayed by OnDomReady and the view's requestHistory listener
		if (_view == 0) {
			return;  // cached; first open replays it once the view exists
		}
		if (IsHealthy()) {
			_api->InteropCall(_view, "setFontSize", std::to_string(_fontSize).c_str());
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

	void PrismaViewBridge::SetFontSize(int fontSize) noexcept
	{
		_fontSize = fontSize;
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
