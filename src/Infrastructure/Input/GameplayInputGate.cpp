#include "GameplayInputGate.h"

#include <format>

namespace F4DH::Infrastructure
{
	namespace
	{
		// Disable set: movement, camera, and combat-adjacent gameplay input.
		// kMenu (1<<3) and kConsole (1<<4) are deliberately excluded — menus
		// and the console must keep working while the search box is focused.
		constexpr auto kBlockedUserEvents = static_cast<RE::UserEvents::USER_EVENT_FLAG>(
			static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kMovement) |
			static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kLooking) |
			static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kActivate) |
			static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kPOVSwitch) |
			static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kFighting) |
			static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kSneaking) |
			static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kWheelZoom) |
			static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kJumping) |
			static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kVATS));
	}

	GameplayInputGate::GameplayInputGate(Core::ILogger& log, bool enabled) :
		_log(log),
		_enabled(enabled)
	{}

	void GameplayInputGate::Initialize() noexcept
	{
		if (!_enabled) {
			return;
		}
		const auto manager = RE::BSInputEnableManager::GetSingleton();
		if (!manager) {
			_log.Warn("gameplay input gate: BSInputEnableManager unavailable — WASD/camera stay live while typing");
			return;
		}
		if (!manager->AllocateNewLayer(_layer, "FO4DialogueHistory typing")) {
			_log.Warn("gameplay input gate: input-enable layer allocation failed — WASD/camera stay live while typing");
			return;
		}
		_manager = manager;
		_log.Info(std::format("gameplay input gate: input-enable layer {} allocated", _layer->GetLayerID()));
	}

	void GameplayInputGate::BlockGameplayInput() noexcept
	{
		SetBlocked(true);
	}

	void GameplayInputGate::UnblockGameplayInput() noexcept
	{
		SetBlocked(false);
	}

	void GameplayInputGate::SetBlocked(bool block) noexcept
	{
		if (!_enabled || !_manager || _blocked == block) {
			return;
		}
		_manager->EnableUserEvent(_layer->GetLayerID(), kBlockedUserEvents, !block, RE::UserEvents::SENDER_ID::kGameplay);
		_blocked = block;
		_log.Info(block ? "gameplay input blocked (typing)" : "gameplay input unblocked");
	}
}
