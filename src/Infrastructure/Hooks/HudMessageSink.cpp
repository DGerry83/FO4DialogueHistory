#include "HudMessageSink.h"

#include "Application/Capture/IMessageSink.h"

#include "REL/HookVft.hpp"

#include "RE/B/BSSpinLock.hpp"
#include "RE/H/HUDDataModel.hpp"

#include "REX/Log.hpp"

#include <optional>

// Display-data observation through the engine's live HUD data model. The
// published value can include widget protocols as well as player notifications.
// HUDDataModel::PerformInputProcessing runs the original model update first;
// the observer then reads nextMessageData under its lock and emits only value
// transitions. The earlier RTTI-slot registration was invalid, and
// GameUIModel::UpdateDataModels is not the normal HUD update path.
//
// Hot path: unchanged or value-less updates perform pointer/flag comparisons
// only. Strings and MessageEvent allocation happen only for a new nonempty
// display value. Engine data is never mutated.

namespace
{
	using PerformInputProcessing_t = void(RE::HUDDataModel*, const RE::InputEvent*);

	F4DH::Application::IMessageSink* g_sink{ nullptr };
	PerformInputProcessing_t*        g_original{ nullptr };
	bool                             g_hadDisplayValue{ false };
	const char*                      g_lastText{ nullptr };
	bool                             g_lastWarning{ false };
	bool                             g_lastRadio{ false };
	bool                             g_lastThrottle{ false };
	bool                             g_verboseDiagnostics{ false };
	std::uint32_t                    g_updateCount{ 0 };

	void LogUpdate()
	{
		if (!g_verboseDiagnostics) {
			return;
		}
		++g_updateCount;
		if (g_updateCount == 1 || g_updateCount % 600 == 0) {
			REX::LogInformation("HudMessageSink diagnostic: HUDDataModel input update #{}", g_updateCount);
		}
	}

	void ObserveDisplayValue(RE::HUDDataModel* a_hud)
	{
		if (!a_hud) {
			g_hadDisplayValue = false;
			g_lastText = nullptr;
			return;
		}

		std::optional<F4DH::Application::MessageEvent> event;
		{
			auto& display = a_hud->messagesModel.nextMessageData;
			const auto displayLock = RE::BSAutoLock(display.dataLock);
			if (!display.optionalValue.has_value()) {
				g_hadDisplayValue = false;
				g_lastText = nullptr;
				return;
			}

			const auto& show = display.optionalValue.value();
			const char* text = show.MessageText.c_str();
			const bool unchanged = g_hadDisplayValue && g_lastText == text &&
				g_lastWarning == show.IsWarningMessage &&
				g_lastRadio == show.IsRadioMessage &&
				g_lastThrottle == show.CanBeThrottled;
			if (unchanged) {
				return;
			}

			g_hadDisplayValue = true;
			g_lastText = text;
			g_lastWarning = show.IsWarningMessage;
			g_lastRadio = show.IsRadioMessage;
			g_lastThrottle = show.CanBeThrottled;

			if (!g_sink || !text || text[0] == '\0') {
				return;
			}

			event.emplace();
			event->text = text;
			event->isWarningMessage = show.IsWarningMessage;
			event->isRadioMessage = show.IsRadioMessage;
			event->canBeThrottled = show.CanBeThrottled;
		}

		if (g_verboseDiagnostics) {
			REX::LogInformation("HudMessageSink diagnostic: display value transition '{}'", event->text.substr(0, 60));
		}
		g_sink->OnMessage(*event);
	}

	void thunk(RE::HUDDataModel* a_hud, const RE::InputEvent* a_queueHead)
	{
		g_original(a_hud, a_queueHead);
		LogUpdate();
		ObserveDisplayValue(a_hud);
	}
}

namespace F4DH::Infrastructure
{
	bool HudMessageSink::Install(Application::IMessageSink& a_sink, bool a_verboseDiagnostics)
	{
		if (g_sink) {
			return true;
		}
		g_verboseDiagnostics = a_verboseDiagnostics;

		static std::shared_ptr<REL::HookVft<PerformInputProcessing_t>> hook;
		hook = std::make_shared<REL::HookVft<PerformInputProcessing_t>>(
			"HUDDataModel::PerformInputProcessing", RE::HUDDataModel::VTABLE[0], 0, &thunk);
		if (!hook->Init() || !hook->Enable()) {
			REX::LogError("HudMessageSink: failed to install HUDDataModel::PerformInputProcessing observer — message capture disabled");
			return false;
		}
		g_original = hook->get();

		g_sink = &a_sink;
		REX::LogInformation("HudMessageSink: HUDDataModel::PerformInputProcessing observer installed (HUD display-data capture active)");
		return true;
	}
}
