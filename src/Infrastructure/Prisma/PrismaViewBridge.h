#pragma once

#include <optional>

#include "Application/View/IViewBridge.h"
#include "Core/Geometry/PanelGeometry.h"
#include "Core/Settings/ViewFilterState.h"
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
		void SetCloseCallback(CloseCallback fn) override;
		void SetClearCallbacks(ClearAllCallback allFn, ClearQuestCallback questFn) override;
		void SetClearQuestsCallback(ClearQuestsCallback questsFn) override;
		[[nodiscard]] bool IsHealthy() override;

		// Configured base font size, pushed to the view ahead of each
		// snapshot (C5 wiring; set once after the INI load).
		void SetFontSize(int fontSize) noexcept;

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

	private:
		PRISMA_UI_API::IVPrismaUI11*       _api = nullptr;  // resolved at construction; null = PrismaUI absent
		PrismaView                         _view = 0;
		CloseCallback                      _closeCallback = nullptr;
		ClearAllCallback                   _clearAllCallback = nullptr;
		ClearQuestCallback                 _clearQuestCallback = nullptr;
		ClearQuestsCallback                _clearQuestsCallback = nullptr;
		int                                _fontSize{ 16 };
		std::optional<Core::PanelGeometry> _geometry;
		Core::ViewFilterState              _filters;
		Application::ISettingsStore*       _settingsStore = nullptr;
	};
}
