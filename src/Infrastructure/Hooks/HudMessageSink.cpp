#include "HudMessageSink.h"

#include "Application/Capture/IMessageSink.h"

#include "REL/REL.hpp"

#include "RE/B/BSSpinLock.hpp"
#include "RE/G/GameUIModel.hpp"
#include "RE/H/HUDDataModel.hpp"

#include "REX/Log.hpp"

#include <cstring>

// Accepted-display observation through the engine's live HUD data model.
// GameUIModel::UpdateDataModels runs the original model update first; the
// observer then reads nextMessageData under its lock and emits only value
// transitions. The earlier RTTI-slot registration was invalid: RTTI metadata
// is not the global EventSource storage and must never be dereferenced as such.
//
// Hot path: unchanged or value-less updates perform pointer/flag comparisons
// only. Strings and MessageEvent allocation happen only for a new nonempty
// display value. Engine data is never mutated.

namespace
{
	using UpdateDataModels_t = void(RE::GameUIModel*);

	constexpr auto kUpdateDataModels = RE::ID::GameUIModel::UpdateDataModels;
	constexpr std::size_t kStolenBytes{ sizeof(REL::Asm::Jump5) };

	F4DH::Application::IMessageSink* g_sink{ nullptr };
	UpdateDataModels_t*              g_original{ nullptr };
	bool                             g_hadDisplayValue{ false };
	const char*                      g_lastText{ nullptr };
	bool                             g_lastWarning{ false };
	bool                             g_lastRadio{ false };
	bool                             g_lastThrottle{ false };

	class HookPrologue5 final : public REL::IHookFn<UpdateDataModels_t>
	{
	public:
		using value_type = std::decay_t<UpdateDataModels_t>;

		HookPrologue5(std::string a_name, const REL::IId& a_id, const value_type& a_newFunc) :
			IHookFn(std::move(a_name), a_id, 0, a_newFunc)
		{
			InitData();
		}

		~HookPrologue5() noexcept override = default;

		HookPrologue5(const HookPrologue5&) = delete;
		HookPrologue5(HookPrologue5&&) noexcept = default;
		HookPrologue5& operator=(const HookPrologue5&) = delete;
		HookPrologue5& operator=(HookPrologue5&&) noexcept = default;

		[[nodiscard]] std::string_view GetTypeName() const noexcept override { return "Prologue5"; }
		[[nodiscard]] value_type GetContinuation() const noexcept { return _continuation; }

	protected:
		void InitNewBytes() override
		{
			const auto assembly = REL::Asm::Jump5(
				this->_address,
				REL::GetTrampoline()->AllocateBranch5(this->_newFunc.GetAddress()));
			REL::WriteData(std::span(this->_newBytes), assembly);

			auto* continuation = std::bit_cast<std::byte*>(_continuation);
			std::memcpy(continuation, this->_oldBytes.data(), kStolenBytes);
			const REL::Asm::Jump14 jmpBack(this->_address + kStolenBytes);
			std::memcpy(continuation + kStolenBytes, &jmpBack, sizeof(jmpBack));
		}

	private:
		void InitData()
		{
			this->_size = kStolenBytes;
			this->_trampolineSize = sizeof(REL::Asm::Jump14) + kStolenBytes + sizeof(REL::Asm::Jump14);

			const auto continuation = REL::GetTrampoline()->Allocate(kStolenBytes + sizeof(REL::Asm::Jump14));
			const auto continuationAddr = std::bit_cast<std::uintptr_t>(continuation);
			_continuation = std::bit_cast<value_type>(continuationAddr);
			this->_oldFunc = continuationAddr;
		}

		value_type _continuation{ nullptr };
	};

	void ObserveDisplayValue()
	{
		auto* hud = RE::HUDDataModel::GetSingleton();
		if (!hud) {
			g_hadDisplayValue = false;
			g_lastText = nullptr;
			return;
		}

		auto& display = hud->messagesModel.nextMessageData;
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

		F4DH::Application::MessageEvent event;
		event.text = text;
		event.isWarningMessage = show.IsWarningMessage;
		event.isRadioMessage = show.IsRadioMessage;
		event.canBeThrottled = show.CanBeThrottled;
		g_sink->OnMessage(event);
	}

	void thunk(RE::GameUIModel* a_model)
	{
		g_original(a_model);
		ObserveDisplayValue();
	}
}

namespace F4DH::Infrastructure
{
	bool HudMessageSink::Install(Application::IMessageSink& a_sink)
	{
		if (g_sink) {
			return true;
		}

		auto& trampoline = REL::GetTrampoline();
		if (trampoline->IsEmpty()) {
			trampoline->Create(1 << 8);
		}

		static std::shared_ptr<HookPrologue5> hook;
		hook = std::make_shared<HookPrologue5>("GameUIModel::UpdateDataModels", kUpdateDataModels, &thunk);
		if (!hook->Init()) {
			REX::LogError("HudMessageSink: failed to install GameUIModel::UpdateDataModels observer — message capture disabled");
			return false;
		}
		g_original = hook->GetContinuation();

		g_sink = &a_sink;
		REX::LogInformation("HudMessageSink: GameUIModel::UpdateDataModels observer installed (accepted-display capture active)");
		return true;
	}
}
