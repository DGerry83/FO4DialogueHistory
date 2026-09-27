#pragma once

#include "IViewBridge.h"

namespace F4DH::Application
{
	// Degraded-mode bridge: every operation is a no-op, Create reports
	// unavailable. Used when PrismaUI F4 is absent.
	class NullViewBridge final : public IViewBridge
	{
	public:
		[[nodiscard]] bool Create() override { return false; }
		void Show() override {}
		void Hide() override {}
		void Focus() override {}
		void Unfocus() override {}
		void PushSnapshot(const std::string&) override {}
		void AppendLine(const std::string&) override {}
		void SetCloseCallback(CloseCallback) override {}
		[[nodiscard]] bool IsHealthy() override { return false; }
	};
}
