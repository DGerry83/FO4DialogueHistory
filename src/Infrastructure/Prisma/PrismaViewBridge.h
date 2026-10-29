#pragma once

#include <functional>
#include <optional>

#include "Application/View/IViewBridge.h"
#include "Core/Geometry/PanelGeometry.h"
#include "Core/Settings/ViewFilterState.h"
#include "Core/Settings/ViewPreferences.h"
#include "PrismaUI_F4_API.h"

namespace F4DH::Application
{
	class ISettingsStore;
}

namespace F4DH::Infrastructure
{
	// IViewBridge over the PrismaUI F4 runtime API. The API is resolved with
	// GetProcAddress on the already-loaded PrismaUI_F4.dll — never linked.
	class PrismaViewBridge final : public Application::IViewBridge
	{
	public:
		PrismaViewBridge();

		[[nodiscard]] bool Create() override;
		void Show() override;
		void Hide() override;
		void Focus() override;
		void Unfocus() override;
		void PushSnapshot(const std::string& json) override;
		void AppendLine(const std::string& json) override;
		void PushMessageSnapshot(const std::string& json) override;
		void AppendMessage(const std::string& json) override;
		void PushNoteSnapshot(const std::string& json) override;
		void PushNoteBody(const std::string& json) override;
		void SetCloseCallback(CloseCallback fn) override;
		void SetClearCallbacks(ClearAllCallback allFn, ClearQuestCallback questFn) override;
		void SetClearQuestsCallback(ClearQuestsCallback questsFn) override;
		void SetClearMessagesCallback(ClearMessagesCallback fn) override;
		void SetClearNotesCallback(ClearNotesCallback fn) override;
		void SetNoteBodyRequestCallback(NoteBodyRequestCallback fn) override;
		void SetNoteSearchRequestCallback(NoteSearchRequestCallback fn) override;
		[[nodiscard]] bool IsHealthy() override;

		// #005: the view reports a text input focused ("1") / blurred ("0")
		// via the typingStateChanged listener; wired by Composition to
		// ViewController::SetTyping. Invoked on the game thread.
		using TypingStateCallback = std::function<void(bool)>;
		void SetTypingStateCallback(TypingStateCallback fn);

		// True only when the runtime exposed IVPrismaUI12 (controller-action
		// binding); false on the V11 fallback. Hard gate for any V12-only
		// call — C7 binds controller actions only when this is true.
		[[nodiscard]] bool ControllerActionsAvailable() const;

		// Configured base font size, pushed to the view ahead of each
		// snapshot (C5 wiring; set once after the INI load). Seeds the
		// cached preference value; SetViewPreferences supersedes it.
		void SetFontSize(int fontSize) noexcept;

		// View preferences (font size + pause-on-open). SetViewPreferences
		// seeds from the INI at composition time and caches the latest
		// committed state; PauseOnOpen exposes the cached flag to the
		// OnDomReady focus replay.
		void SetViewPreferences(const Core::ViewPreferences& preferences) noexcept;
		[[nodiscard]] bool PauseOnOpen() const noexcept;

		// M5: persisted panel geometry. SetGeometry seeds from the INI at
		// composition time and caches the latest view report; SetSettingsSink
		// is the persistence target for view-reported geometry.
		void SetGeometry(const std::optional<Core::PanelGeometry>& geometry) noexcept;
		void SetSettingsSink(Application::ISettingsStore* store) noexcept;

		// Game-thread entry for a view geometryChanged report: tolerant parse,
		// cache update, persistence. Called from the listener's Dispatch.
		void HandleGeometryReport(const std::string& jsonPayload);

		// InteropCall setGeometry when healthy + geometry present. Called on
		// the game thread from PushSnapshot and OnDomReady (first-open race).
		void ApplyGeometry();

		// Persisted quest-bucket filter checkboxes. SetFilters seeds from the
		// INI at composition time and caches the latest view report.
		void SetFilters(const Core::ViewFilterState& filters) noexcept;

		// Game-thread entry for a view filtersChanged report: tolerant parse,
		// cache update, persistence. Called from the listener's Dispatch.
		void HandleFiltersReport(const std::string& jsonPayload);

		// InteropCall setFilters when healthy. Called on the game thread from
		// PushSnapshot and OnDomReady (same first-open race as ApplyGeometry).
		void ApplyFilters();

		// Persisted view preferences (font + pause). HandleViewPreferencesReport
		// is the game-thread entry for a view viewPreferencesChanged report:
		// tolerant Core parse into a copy of the cache, cache update, sidecar
		// persistence, and — when the pause value changed while the panel is
		// focused — the ordered Unfocus/Focus refocus (PrismaUI ignores Focus
		// on an already-focused view, so a live pause change needs the balanced
		// cycle). Malformed payloads never alter the cache or the sidecar.
		void HandleViewPreferencesReport(const std::string& jsonPayload);

		// InteropCall setViewPreferences when healthy. Called on the game
		// thread from PushSnapshot and OnDomReady (same first-open race as
		// ApplyGeometry/ApplyFilters). The view keeps the legacy setFontSize
		// entry point for compatibility with older native builds.
		void ApplyViewPreferences();

		// C8: report the active input device via InteropCall setInputDevice so
		// the view can tailor its hint chrome. Runs only when the runtime
		// exposed V12 (IsUsingGamepad is a V9 virtual — safe through the V12
		// pointer while ControllerActionsAvailable() guarantees a real V12
		// object) and the view is healthy (IsHealthy, like every other push
		// path — InteropCall before DOM ready is silently dropped). Called on
		// the game thread from OnDomReady (after the C7 bindings) and Focus()
		// (re-opens report the current device); device changes mid-session
		// reach the view through PrismaUI's own 'prisma-input-device-change'
		// window event instead.
		void PushInputDeviceMode();

	private:
		PRISMA_UI_API::IVPrismaUI12*       _api = nullptr;  // resolved at construction; null = PrismaUI absent; on fallback holds the V11 pointer (downcast)
		bool                               _controllerActionsAvailable{ false };  // true only when V12 was exposed; V12-only calls must check this first
		PrismaView                         _view = 0;
		CloseCallback                      _closeCallback = nullptr;
		ClearAllCallback                   _clearAllCallback = nullptr;
		ClearQuestCallback                 _clearQuestCallback = nullptr;
		ClearQuestsCallback                _clearQuestsCallback = nullptr;
		ClearMessagesCallback              _clearMessagesCallback = nullptr;
		ClearNotesCallback                 _clearNotesCallback = nullptr;
		NoteBodyRequestCallback            _noteBodyRequestCallback = nullptr;
		NoteSearchRequestCallback          _noteSearchRequestCallback = nullptr;
		TypingStateCallback                _typingStateCallback;
		Core::ViewPreferences              _preferences;  // cached view preferences (16/false until seeded); pushed to the view ahead of each snapshot
		bool                               _focused{ false };  // set by Focus(), cleared by Unfocus()/Hide(); gates the live pause-change refocus
		std::optional<Core::PanelGeometry> _geometry;
		Core::ViewFilterState              _filters;
		Application::ISettingsStore*       _settingsStore = nullptr;
	};
}
