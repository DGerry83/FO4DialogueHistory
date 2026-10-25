#include "PrismaViewBridge.h"

#include <charconv>
#include <cstdint>
#include <functional>
#include <system_error>
#include <utility>

#include "Application/Settings/ISettingsStore.h"
#include "Core/Geometry/PanelGeometry.h"
#include "Core/Settings/ViewPreferences.h"

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
		// plain-function callbacks reach it without header changes. Typed as
		// IVPrismaUI12 (the C5 member's type) so OnDomReady can reach the V12
		// controller-action virtuals; on the V11 fallback this holds the
		// downcast pointer — V12 virtuals must stay gated on
		// ControllerActionsAvailable() (calling them on the fallback is UB).
		PrismaViewBridge*                g_bridge{ nullptr };
		PRISMA_UI_API::IVPrismaUI12*     g_api{ nullptr };
		PrismaViewBridge::CloseCallback  g_closeCallback{ nullptr };
		PrismaViewBridge::ClearAllCallback   g_clearAllCallback{ nullptr };
		PrismaViewBridge::ClearQuestCallback g_clearQuestCallback{ nullptr };
		PrismaViewBridge::ClearQuestsCallback g_clearQuestsCallback{ nullptr };
		PrismaViewBridge::TypingStateCallback g_typingStateCallback;  // #005 mirror
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
			REX::LogInformation("PrismaViewBridge: closeRequested received from view");
			Dispatch([] {
				if (g_closeCallback) {
					g_closeCallback();
				}
			});
		}

		void OnClearAllRequested(const char*)
		{
			// Fires on PrismaUI's thread; route on the game thread.
			REX::LogInformation("PrismaViewBridge: clearAllRequested received from view");
			Dispatch([] {
				if (g_clearAllCallback) {
					g_clearAllCallback();
				}
			});
		}

		void OnClearQuestRequested(const char* a_payload)
		{
			// Fires on PrismaUI's thread. Parsing is pure, so validate the
			// decimal questId here and drop malformed payloads before paying
			// for a game-thread hop; the mutation itself runs on the game
			// thread like every other buffer operation.
			const std::string payload = a_payload ? a_payload : "";
			std::uint32_t     questId = 0;
			const auto        parsed = std::from_chars(payload.data(), payload.data() + payload.size(), questId);
			if (parsed.ec != std::errc() || parsed.ptr != payload.data() + payload.size()) {
				REX::LogWarning("PrismaViewBridge: ignoring malformed clearQuestRequested payload '{}'", payload);
				return;
			}
			Dispatch([questId] {
				if (g_clearQuestCallback) {
					g_clearQuestCallback(questId);
				}
			});
		}

		void OnClearQuestsRequested(const char* a_payload)
		{
			// Fires on PrismaUI's thread. Parsing is pure, so split and
			// validate the comma-joined decimal questIds here and drop
			// malformed tokens before paying for a game-thread hop; the
			// mutation itself runs on the game thread like every other buffer
			// operation.
			const std::string          payload = a_payload ? a_payload : "";
			std::vector<std::uint32_t> questIds;
			std::size_t                dropped = 0;

			std::size_t start = 0;
			while (start <= payload.size()) {
				const auto comma = payload.find(',', start);
				const auto end   = comma == std::string::npos ? payload.size() : comma;
				auto       token = std::string_view(payload).substr(start, end - start);
				while (!token.empty() && (token.front() == ' ' || token.front() == '\t' ||
						   token.front() == '\r' || token.front() == '\n')) {
					token.remove_prefix(1);
				}
				while (!token.empty() && (token.back() == ' ' || token.back() == '\t' ||
						   token.back() == '\r' || token.back() == '\n')) {
					token.remove_suffix(1);
				}

				std::uint32_t questId = 0;
				const auto    parsed = std::from_chars(token.data(), token.data() + token.size(), questId);
				if (parsed.ec == std::errc() && parsed.ptr == token.data() + token.size()) {
					questIds.push_back(questId);
				} else {
					dropped += 1;
				}

				if (comma == std::string::npos) {
					break;
				}
				start = comma + 1;
			}

			if (questIds.empty()) {
				REX::LogInformation("PrismaViewBridge: clearQuestsRequested payload '{}' had no valid quest ids — ignoring", payload);
				return;
			}
			if (dropped > 0) {
				REX::LogWarning("PrismaViewBridge: dropped {} malformed quest id token(s) from clearQuestsRequested payload '{}'",
					dropped, payload);
			}
			Dispatch([questIds] {
				if (g_clearQuestsCallback) {
					g_clearQuestsCallback(questIds);
				}
			});
		}

		void OnTypingStateChanged(const char* a_payload)
		{
			// #005: fires on PrismaUI's thread. The payload is exactly "1"
			// (a view text input focused) or "0" (blurred); validate here like
			// the other payloads, apply on the game thread — the typing flag
			// is game-thread state (documented invariant).
			const std::string payload = a_payload ? a_payload : "";
			if (payload != "0" && payload != "1") {
				REX::LogWarning("PrismaViewBridge: ignoring malformed typingStateChanged payload '{}'", payload);
				return;
			}
			const bool typing = payload == "1";
			Dispatch([typing] {
				if (g_typingStateCallback) {
					g_typingStateCallback(typing);
				}
			});
		}

		void OnGeometryChanged(const char* a_json)
		{
			// Fires on PrismaUI's thread; parse + persist on the game thread
			// (M5-G4: INI write at mouseup cadence, user-gesture bounded).
			const std::string payload = a_json ? a_json : "";
			Dispatch([payload] {
				if (g_bridge) {
					g_bridge->HandleGeometryReport(payload);
				}
			});
		}

		void OnFiltersChanged(const char* a_json)
		{
			// Fires on PrismaUI's thread; parse + persist on the game thread
			// (INI write at checkbox-change cadence, user-gesture bounded) —
			// the exact geometryChanged split.
			const std::string payload = a_json ? a_json : "";
			Dispatch([payload] {
				if (g_bridge) {
					g_bridge->HandleFiltersReport(payload);
				}
			});
		}

		void OnViewPreferencesChanged(const char* a_json)
		{
			// Fires on PrismaUI's thread; validate + persist on the game
			// thread (sidecar write at commit cadence: one completed slider
			// gesture / numeric commit / checkbox change). The tolerant parse
			// is pure, but cache mutation and the live pause refocus are
			// game-thread state — the exact filtersChanged split.
			const std::string payload = a_json ? a_json : "";
			Dispatch([payload] {
				if (g_bridge) {
					g_bridge->HandleViewPreferencesReport(payload);
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
			g_api->RegisterJSListener(a_view, "geometryChanged", &OnGeometryChanged);
			g_api->RegisterJSListener(a_view, "filtersChanged", &OnFiltersChanged);
			g_api->RegisterJSListener(a_view, "clearAllRequested", &OnClearAllRequested);
			g_api->RegisterJSListener(a_view, "clearQuestRequested", &OnClearQuestRequested);
			g_api->RegisterJSListener(a_view, "clearQuestsRequested", &OnClearQuestsRequested);
			g_api->RegisterJSListener(a_view, "typingStateChanged", &OnTypingStateChanged);  // #005: text-input focus/blur reports
			g_api->RegisterJSListener(a_view, "viewPreferencesChanged", &OnViewPreferencesChanged);  // font slider / numeric / pause checkbox commits
			// C7: V12 controller actions (gamepad). Bound only when the runtime
			// exposed IVPrismaUI12 — on the V11 fallback g_api is a downcast and
			// these virtuals must never run (ControllerActionsAvailable gates
			// them). B/D-pad are deliberately NOT bound: they stay on the
			// legacy synthetic-key route (D-pad -> arrows, B -> Escape) that the
			// C6 focus model consumes. A MUST be bound (#007/#010): PrismaUI's
			// VirtualPointer intercepts A before the legacy A -> Enter
			// conversion and clicks at the cursor, while bound V12 actions are
			// evaluated first (ControllerGlyphs routing order) — so only a
			// binding makes A reach the view. LT/RT hysteresis is the
			// framework's own (press >= 0.55, release <= 0.45).
			if (g_bridge && g_bridge->ControllerActionsAvailable()) {
				constexpr std::pair<const char*, const char*> kControllerActions[] = {
					{ "A", "activate" },
					{ "X", "clear_quest" },
					{ "Y", "clear_all" },
					{ "LB", "page_up" },
					{ "RB", "page_down" },
					{ "LT", "jump_oldest" },
					{ "RT", "jump_newest" },
					{ "Start", "close" }
				};
				std::size_t bound = 0;
				for (const auto& [button, action] : kControllerActions) {
					if (g_api->BindControllerAction(a_view, button, action)) {
						bound += 1;
					} else {
						REX::LogWarning("PrismaViewBridge: BindControllerAction failed ({} -> {})", button, action);
					}
				}
				REX::LogInformation("PrismaViewBridge: controller actions bound ({})", bound);
			}
			// C8: report the active input device for the hint chrome (V12-only —
			// PushInputDeviceMode re-checks ControllerActionsAvailable() itself).
			if (g_bridge) {
				g_bridge->PushInputDeviceMode();
			}
			g_api->SetViewRole(a_view, PRISMA_UI_API::ViewRole::kPanel);
			g_api->SetViewOwnsEscape(a_view, true);
			// Deterministic snapshot replay: the view's requestHistory can fire
			// before the listener is registered, so push the cache directly.
			if (g_bridge && !g_snapshotCache.empty()) {
				g_bridge->PushSnapshot(g_snapshotCache);
			}
			// Same first-open race for geometry: an empty snapshot cache skips
			// the PushSnapshot path above, so apply it here too.
			if (g_bridge) {
				g_bridge->ApplyGeometry();
				g_bridge->ApplyFilters();
				g_bridge->ApplyViewPreferences();
			}
			if (g_focusPending) {
				g_focusPending = false;
				// Pause only when the cached preference demands it (default
				// off — the panel must never pause an unfocused-by-default game).
				g_api->Focus(a_view, g_bridge ? g_bridge->PauseOnOpen() : false);
			}
		}
	}

	PrismaViewBridge::PrismaViewBridge() :
		_api(PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI12>())
	{
		if (!_api) {
			// V12 not exposed: fall back to V11 — identical V1–V11 surface,
			// minus the controller actions (C7/C8 stay inert via the flag).
			if (auto* v11 = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI11>()) {
				_api = static_cast<PRISMA_UI_API::IVPrismaUI12*>(v11);
				REX::LogWarning("PrismaViewBridge: controller actions disabled — PrismaUI V12 not exposed");
			}
			// Both absent: _api stays null; Create() logs the degraded-path error.
		} else {
			_controllerActionsAvailable = true;
			REX::LogInformation("PrismaViewBridge: PrismaUI V12 acquired — controller actions available");
		}
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
		_focused = false;
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
		_focused = true;  // tracked for the live pause-change refocus
		if (_view == 0) {
			return;  // applied after Create() via OnDomReady
		}
		_api->Focus(_view, _preferences.pauseOnOpen);  // pause only when the preference demands it
		PushInputDeviceMode();  // C8: a re-open reports the current input device
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
		_focused = false;
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
			ApplyGeometry();
			ApplyFilters();
			ApplyViewPreferences();
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

	void PrismaViewBridge::SetClearCallbacks(ClearAllCallback allFn, ClearQuestCallback questFn)
	{
		_clearAllCallback = allFn;
		_clearQuestCallback = questFn;
		g_clearAllCallback = allFn;
		g_clearQuestCallback = questFn;
	}

	void PrismaViewBridge::SetClearQuestsCallback(ClearQuestsCallback questsFn)
	{
		_clearQuestsCallback = questsFn;
		g_clearQuestsCallback = questsFn;
	}

	void PrismaViewBridge::SetTypingStateCallback(TypingStateCallback fn)
	{
		_typingStateCallback = fn;
		g_typingStateCallback = fn;
	}

	void PrismaViewBridge::SetFontSize(int fontSize) noexcept
	{
		_preferences.fontSize = fontSize;
	}

	void PrismaViewBridge::SetViewPreferences(const Core::ViewPreferences& preferences) noexcept
	{
		_preferences = preferences;
	}

	bool PrismaViewBridge::PauseOnOpen() const noexcept
	{
		return _preferences.pauseOnOpen;
	}

	void PrismaViewBridge::SetGeometry(const std::optional<Core::PanelGeometry>& geometry) noexcept
	{
		_geometry = geometry;
	}

	void PrismaViewBridge::SetSettingsSink(Application::ISettingsStore* store) noexcept
	{
		_settingsStore = store;
	}

	void PrismaViewBridge::HandleGeometryReport(const std::string& a_jsonPayload)
	{
		Core::PanelGeometry geometry;
		if (!Core::ParsePanelGeometry(a_jsonPayload, geometry)) {
			REX::LogWarning("PrismaViewBridge: ignoring malformed geometryChanged payload");
			return;
		}
		_geometry = geometry;
		if (_settingsStore) {
			_settingsStore->SaveGeometry(geometry);
		}
		REX::LogInformation("PrismaViewBridge: panel geometry {}x{} at ({},{})",
			geometry.width, geometry.height, geometry.x, geometry.y);
	}

	void PrismaViewBridge::ApplyGeometry()
	{
		if (_api && _view != 0 && _geometry && IsHealthy()) {
			_api->InteropCall(_view, "setGeometry", Core::FormatPanelGeometry(*_geometry).c_str());
		}
	}

	void PrismaViewBridge::SetFilters(const Core::ViewFilterState& filters) noexcept
	{
		_filters = filters;
	}

	void PrismaViewBridge::HandleFiltersReport(const std::string& a_jsonPayload)
	{
		Core::ViewFilterState filters;
		if (!Core::ParseViewFilterState(a_jsonPayload, filters)) {
			REX::LogWarning("PrismaViewBridge: ignoring malformed filtersChanged payload");
			return;
		}
		_filters = filters;
		if (_settingsStore) {
			_settingsStore->SaveViewFilters(filters);
		}
		REX::LogInformation("PrismaViewBridge: view filters changed (main={} side={} misc={} unattributed={})",
			filters.main, filters.side, filters.misc, filters.unattributed);
	}

	void PrismaViewBridge::ApplyFilters()
	{
		if (_api && _view != 0 && IsHealthy()) {
			_api->InteropCall(_view, "setFilters", Core::FormatViewFilterState(_filters).c_str());
		}
	}

	void PrismaViewBridge::HandleViewPreferencesReport(const std::string& a_jsonPayload)
	{
		// Tolerant parse into a copy of the cache: a malformed or junk payload
		// returns false with the copy untouched, so neither the cached state
		// nor the sidecar ever sees it (contract invariant).
		Core::ViewPreferences parsed = _preferences;
		if (!Core::ParseViewPreferences(a_jsonPayload, parsed)) {
			REX::LogWarning("PrismaViewBridge: ignoring malformed viewPreferencesChanged payload");
			return;
		}
		const bool pauseChanged = parsed.pauseOnOpen != _preferences.pauseOnOpen;
		_preferences = parsed;
		if (_settingsStore) {
			_settingsStore->SaveViewPreferences(_preferences);
		}
		REX::LogInformation("PrismaViewBridge: view preferences changed (fontSize={} pauseOnOpen={})",
			_preferences.fontSize, _preferences.pauseOnOpen);
		if (pauseChanged && _focused && _view != 0 && IsHealthy()) {
			// Live pause change: PrismaUI ignores Focus() on an already-focused
			// view, so the new value needs the balanced cycle — Unfocus first,
			// then Focus with the new pause flag. PrismaUI owns the engine
			// pause count; every close path still runs Unfocus before Hide.
			_api->Unfocus(_view);
			_api->Focus(_view, _preferences.pauseOnOpen);
		}
	}

	void PrismaViewBridge::ApplyViewPreferences()
	{
		if (_api && _view != 0 && IsHealthy()) {
			_api->InteropCall(_view, "setViewPreferences", Core::FormatViewPreferences(_preferences).c_str());
		}
	}

	void PrismaViewBridge::PushInputDeviceMode()
	{
		// IsUsingGamepad is a V9 virtual: callable through the V12 pointer only
		// while _controllerActionsAvailable guarantees a real V12 object — on
		// the V11 fallback _api is a downcast and any V9+ virtual is UB.
		if (_api && _controllerActionsAvailable && _view != 0 && IsHealthy()) {
			const bool gamepad = _api->IsUsingGamepad();
			_api->InteropCall(_view, "setInputDevice",
				gamepad ? "{\"gamepad\":true}" : "{\"gamepad\":false}");
		}
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

	bool PrismaViewBridge::ControllerActionsAvailable() const
	{
		return _controllerActionsAvailable;
	}
}
