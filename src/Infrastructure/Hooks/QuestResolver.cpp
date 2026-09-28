#include "QuestResolver.h"

#include "Core/Dialogue/AliasTokens.h"

#include "RE/B/BGSLocAlias.hpp"
#include "RE/B/BGSLocation.hpp"
#include "RE/B/BGSRefAlias.hpp"
#include "RE/B/BGSScene.hpp"
#include "RE/B/BSSpinLock.hpp"
#include "RE/T/TESNPC.hpp"
#include "RE/T/TESObjectREFR.hpp"
#include "RE/T/TESQuest.hpp"
#include "RE/T/TESTopic.hpp"
#include "RE/T/TESTopicInfo.hpp"

#include "REX/Log.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace
{
	void LogOncePerQuest(std::uint32_t a_questId, const std::string& a_questName, const char* a_via)
	{
		// Game-thread only (called from the ShowSubtitle thunk path) — the
		// seen-set needs no lock. Bounded by distinct quests, not line count.
		static std::unordered_set<std::uint32_t> seen;
		if (!seen.insert(a_questId).second) {
			return;
		}
		REX::LogInformation("quest resolved: '{}' (id {:08X}) via {}", a_questName, a_questId, a_via);
	}

	[[nodiscard]] bool IEquals(std::string_view a_lhs, std::string_view a_rhs) noexcept
	{
		if (a_lhs.size() != a_rhs.size()) {
			return false;
		}
		for (std::size_t i = 0; i < a_lhs.size(); ++i) {
			const auto lower = [](char c) noexcept {
				return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
			};
			if (lower(a_lhs[i]) != lower(a_rhs[i])) {
				return false;
			}
		}
		return true;
	}

	// Resolves one "<alias=name>" token against the quest's filled aliases.
	// OG-safe chain only (M7 research): alias array under the read lock is
	// inline member iteration; GetAliasedRef is OG-verified REL::ID 847223;
	// the ref's base object is a member read; the actor name comes from the
	// TESFullName virtual (GetDisplayName/GetDisplayFullName are absent from
	// the OG Address Library DB and must not be called). The engine call runs
	// AFTER the read lock is released. Any miss keeps the token verbatim.
	[[nodiscard]] std::optional<std::string> ResolveAliasName(RE::TESQuest* a_quest, std::string_view a_tokenName)
	{
		std::uint32_t    aliasID = 0;
		bool             isRefAlias = false;
		RE::BGSLocation* forcedLoc = nullptr;
		bool             found = false;
		{
			const auto lock = RE::BSAutoReadLock(a_quest->aliasAccessLock);
			for (const auto* alias : a_quest->aliases) {
				const char* aliasName = alias ? alias->aliasName.c_str() : nullptr;
				if (!aliasName || !IEquals(aliasName, a_tokenName)) {
					continue;
				}
				found = true;
				aliasID = alias->aliasID;
				if (alias->Is<RE::BGSRefAlias>()) {
					isRefAlias = true;
				} else if (const auto* locAlias = alias->As<RE::BGSLocAlias>()) {
					forcedLoc = locAlias->forcedLocation;
				}
				break;
			}
		}
		if (!found) {
			return std::nullopt;
		}

		if (isRefAlias) {
			RE::ObjectRefHandle handle;
			handle = a_quest->GetAliasedRef(&handle, aliasID);
			const auto ref = handle.get();
			if (!ref) {
				return std::nullopt;  // alias unfilled or ref unloaded
			}
			const auto base = ref->GetBaseObject();
			if (base && base->Is(RE::FormType::kActor)) {
				if (const char* name = static_cast<RE::TESNPC*>(base)->GetFullName(); name && name[0] != '\0') {
					return std::string(name);
				}
			}
			return std::nullopt;
		}
		if (forcedLoc) {
			if (const char* name = forcedLoc->GetFullName(); name && name[0] != '\0') {
				return std::string(name);
			}
		}
		return std::nullopt;
	}
}

namespace F4DH::Infrastructure
{
	QuestResolution QuestResolver::Resolve(RE::TESTopicInfo* a_topicInfo)
	{
		QuestResolution result;
		if (!a_topicInfo) {
			return result;
		}

		// The only engine call on this path (C8 rule: OG-verified variant ID).
		const auto scene = a_topicInfo->GetScene();

		RE::TESQuest* quest = nullptr;
		const char*   via = nullptr;
		if (scene && scene->parentQuest) {
			quest = scene->parentQuest;
			via = "scene";
		} else if (const auto topic = a_topicInfo->parentTopic; topic && topic->ownerQuest) {
			quest = topic->ownerQuest;
			via = "topic";
		}
		if (!quest) {
			return result;
		}

		result.questId = quest->formID;
		if (const char* name = quest->fullName.data(); name && name[0] != '\0') {
			result.questName = name;
			// Radiant quests carry raw "<alias=...>" placeholders in fullName
			// (M7); substitute what can be resolved, keep the rest verbatim.
			if (Core::AliasTokens::ContainsToken(result.questName)) {
				result.questName = Core::AliasTokens::Substitute(result.questName, [&](std::string_view token) {
					return ResolveAliasName(quest, token);
				});
			}
		} else if (const char* editorID = quest->GetFormEditorID(); editorID && editorID[0] != '\0') {
			result.questName = editorID;
		}

		LogOncePerQuest(result.questId, result.questName, via);
		return result;
	}
}
