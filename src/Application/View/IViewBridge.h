#pragma once

#include <string>

namespace F4DH::Application
{
	// Abstracts show/hide/focus/push operations on the UI view.
	class IViewBridge
	{
	public:
		using CloseCallback = void (*)();

		virtual ~IViewBridge() = default;

		[[nodiscard]] virtual bool Create() = 0;  // false = unavailable (degraded)
		virtual void Show() = 0;
		virtual void Hide() = 0;
		virtual void Focus() = 0;
		virtual void Unfocus() = 0;
		virtual void PushSnapshot(const std::string& json) = 0;
		virtual void AppendLine(const std::string& json) = 0;
		virtual void SetCloseCallback(CloseCallback fn) = 0;
		[[nodiscard]] virtual bool IsHealthy() = 0;
	};
}
