#pragma once

#include "Application/View/IViewBridge.h"
#include "PrismaUI_F4_API.h"

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
		[[nodiscard]] bool IsHealthy() override;

		// Configured base font size, pushed to the view ahead of each
		// snapshot (C5 wiring; set once after the INI load).
		void SetFontSize(int fontSize) noexcept;

	private:
		PRISMA_UI_API::IVPrismaUI11* _api = nullptr;  // resolved at construction; null = PrismaUI absent
		PrismaView                   _view = 0;
		CloseCallback                _closeCallback = nullptr;
		int                          _fontSize{ 16 };
	};
}
