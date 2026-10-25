#include "HudMessageSink.h"

#include "Application/Capture/IMessageSink.h"

#include "REL/Relocation.hpp"

#include "RE/B/BSTEvent.hpp"
#include "RE/H/HUDMessageDisplayDataEvent.hpp"
#include "RE/IDs_RTTI.hpp"

#include "REX/Log.hpp"

// HUD message display-data observation (Message History v1.3.0).
//
// Code-evidence summary (see the session Phase-0 record): the AV fork exposes
// no plain event source for HUDMessageDisplayDataEvent — it is a
// BSTValueEvent<ShowHUDMessageEvent> whose only header-visible carrier is the
// HUDMessagesModel's pull-model BSTValueEventSource. The engine nonetheless
// publishes every accepted-display value on the global event bus: the desktop
// database lists BSTGlobalEvent::Notify<HUDMessageDisplayDataEvent> /
// RegisterSink<HUDMessageDisplayDataEvent> instantiations and the
// BSTValueEventSource<HUDMessageDisplayDataEvent>::SetValue/BroadcastEvent
// pair, and the fork's own RTTI tables carry the global source's SDM
// static-buffer slot IDs for OG/NG/AE. Subscribing a BSTEventSink on that
// global source is therefore the passive accepted-display boundary the
// approved design selects. The sibling submission event
// (ShowHUDMessageEvent) is header-evidenced as the pre-throttle path
// (HUDMessagesModel queues it into queuedMessages/throttles) and is NOT used.
//
// Threading: the broadcast runs on whichever engine thread updates the HUD
// model; the sink does nothing but copy text into owned storage and hand it
// to the Application pipeline (mutex-guarded buffer; the Prisma bridge
// self-marshals to the game thread). Runtime delivery verification is a G1
// item.

namespace
{
	using EventSource_t = RE::BSTGlobalEvent::EventSource<RE::HUDMessageDisplayDataEvent>;

	// Address Library variant IDs of the global source's SDM static-buffer
	// slot (the storage that holds the EventSource<HUDMessageDisplayDataEvent>*
	// the engine Notifies). Sourced from the AV fork's own RTTI tables
	// (RE/IDs_RTTI.hpp) and cross-checked against the desktop database dump;
	// the same ID namespace as the project's verified ShowSubtitle entry.
	constexpr auto kDisplayDataSourceSlot = RE::RTTI::BSTSingletonSDMOpStaticBuffer_BSTGlobalEvent__EventSource_HUDMessageDisplayDataEvent__;

	F4DH::Application::IMessageSink* g_sink{ nullptr };

	class DisplayDataSink final : public RE::BSTEventSink<RE::HUDMessageDisplayDataEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::HUDMessageDisplayDataEvent& a_event, RE::BSTEventSource<RE::HUDMessageDisplayDataEvent>*) override
		{
			// A cleared/absent value (HUD hiding the current message) is not a
			// display occurrence. Never mutate or consume: kContinue always.
			if (g_sink && a_event.optionalValue.has_value()) {
				const auto& show = a_event.optionalValue.value();

				F4DH::Application::MessageEvent event;
				event.text = show.MessageText.c_str() ? show.MessageText.c_str() : "";
				event.isWarningMessage = show.IsWarningMessage;
				event.isRadioMessage = show.IsRadioMessage;
				event.canBeThrottled = show.CanBeThrottled;

				g_sink->OnMessage(event);
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	// Static storage duration: zero runtime allocation, alive for the process
	// lifetime (F4SE plugins never unload).
	DisplayDataSink g_displayDataSink;
}

namespace F4DH::Infrastructure
{
	bool HudMessageSink::Install(Application::IMessageSink& a_sink)
	{
		if (g_sink) {
			return true;  // idempotent — installed for process lifetime
		}

		static const auto storage = REL::Relocation<EventSource_t**>{ kDisplayDataSourceSlot };
		auto*             source = *storage;
		if (!source) {
			// Engine has not created the source yet (no HUD message ever
			// displayed / subsystem not initialized): create it through the
			// SDM killer exactly like the proven libxse global-event pattern,
			// so the engine's later Notify<T> calls reach this instance.
			auto* global = RE::BSTGlobalEvent::GetSingleton();
			if (!global) {
				REX::LogError("HudMessageSink: BSTGlobalEvent unavailable — message capture disabled");
				return false;
			}
			source = new EventSource_t(global->eventSourceSDMKiller);
			*storage = source;
			REX::LogInformation("HudMessageSink: created HUDMessageDisplayDataEvent global event source");
		}

		if (!source->RegisterSink(&g_displayDataSink)) {
			REX::LogError("HudMessageSink: failed to register display-data sink — message capture disabled");
			return false;
		}

		g_sink = &a_sink;
		REX::LogInformation("HudMessageSink: HUD message display-data sink registered (accepted-display capture active)");
		return true;
	}
}
