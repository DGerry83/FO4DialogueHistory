#include "QuestResolver.h"

#include "Core/Dialogue/AliasTokens.h"

#include "RE/B/BGSLocAlias.hpp"
#include "RE/B/BGSLocation.hpp"
#include "RE/B/BGSQuestInstanceText.hpp"
#include "RE/B/BGSRefAlias.hpp"
#include "RE/B/BGSScene.hpp"
#include "RE/B/BSSpinLock.hpp"
#include "RE/T/TESNPC.hpp"
#include "RE/T/TESObjectREFR.hpp"
#include "RE/T/TESQuest.hpp"
#include "RE/T/TESTopic.hpp"
#include "RE/T/TESTopicInfo.hpp"

#include "REX/Log.hpp"

#include <format>
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
	// One bounded diagnostic line per quest+token pair (game thread only —
	// the seen-set needs no lock). Exists to pin down which resolution step
	// misses for radiant tokens in the field.
	void LogAliasOnce(std::uint32_t a_questId, std::string_view a_token, const std::string& a_detail)
	{
		static std::unordered_set<std::string> seen;
		const std::string key = std::format("{:08X}:{}", a_questId, a_token);
		if (!seen.insert(key).second) {
			return;
		}
		REX::LogInformation("alias resolve [{}]: {}", key, a_detail);
	}

	// OG-safe chain only: alias array under the read lock is inline member
	// iteration; GetAliasedRef is OG-verified REL::ID 847223 (proven stable
	// in-session); the ref's base object is a member read; names come from
	// the TESFullName virtual (GetDisplayName/GetDisplayFullName are absent
	// from the OG Address Library DB and must not be called;
	// BGSQuestInstanceText::ParseString — tried in a0998eb — CTD'd on a
	// radiant-quest conversation despite its ID being present in the OG bin).
	// The engine call runs AFTER the read lock is released. Any miss keeps
	// the token verbatim.
	[[nodiscard]] std::optional<std::string> ResolveAliasName(RE::TESQuest* a_quest, std::string_view a_tokenName)
	{
		std::uint32_t    aliasID = 0;
		bool             isRefAlias = false;
		const char*      aliasType = "other";
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
					aliasType = "Ref";
				} else if (const auto* locAlias = alias->As<RE::BGSLocAlias>()) {
					aliasType = "Loc";
					forcedLoc = locAlias->forcedLocation;
				}
				break;
			}
		}
		if (!found) {
			LogAliasOnce(a_quest->formID, a_tokenName, "no alias with that name");
			return std::nullopt;
		}

		// Ref alias: the aliased actor's name (proven path). A non-actor ref
		// alias is NOT terminal — radiant quests often alias a location's
		// marker ref, so fall through to the location probe below.
		if (isRefAlias) {
			RE::ObjectRefHandle handle;
			handle = a_quest->GetAliasedRef(&handle, aliasID);
			if (const auto ref = handle.get()) {
				const auto base = ref->GetBaseObject();
				if (base && base->Is(RE::FormType::kActor)) {
					if (const char* name = static_cast<RE::TESNPC*>(base)->GetFullName(); name && name[0] != '\0') {
						LogAliasOnce(a_quest->formID, a_tokenName, std::format("aliasID={} type=Ref -> actor '{}'", aliasID, name));
						return std::string(name);
					}
				}
			}
		}

		// Location resolution: forced fill first (M7), then the quest
		// instance data for conditional fills (what radiant quests use).
		const RE::BGSLocation* loc = forcedLoc;
		std::uint32_t        nameFormID = 0;
		if (!loc) {
			// Instance text maps aliasID -> the form providing the display
			// name. All member reads — no engine calls.
			for (const auto* instance : a_quest->instanceDataArray) {
				if (!instance) {
					continue;
				}
				for (const auto& sd : instance->stringDataArray) {
					if (sd.aliasID == aliasID) {
						nameFormID = sd.fullNameFormID;
						break;
					}
				}
				if (nameFormID != 0) {
					break;
				}
			}
			// aliasedLocMap holds the runtime-filled locations. Its key
			// semantics are undocumented, so every hit must be validated:
			// probe either candidate key and require the hit's own formID to
			// match the naming form; as a last resort accept the map's
			// single entry when there is exactly one.
			if (nameFormID != 0) {
				for (const std::uint32_t key : { nameFormID, aliasID }) {
					const auto it = a_quest->aliasedLocMap.find(key);
					if (it != a_quest->aliasedLocMap.end() && it->second &&
						it->second->Is(RE::FormType::kLocation) && it->second->formID == nameFormID) {
						loc = it->second;
						break;
					}
				}
			}
			if (!loc && a_quest->aliasedLocMap.size() == 1) {
				const auto it = a_quest->aliasedLocMap.begin();
				if (it != a_quest->aliasedLocMap.end() && it->second &&
					it->second->Is(RE::FormType::kLocation)) {
					loc = it->second;
				}
			}
		}
		if (loc) {
			if (const char* name = loc->GetFullName(); name && name[0] != '\0') {
				LogAliasOnce(a_quest->formID, a_tokenName, std::format("aliasID={} type={} nameFormID={:08X} -> loc '{}'", aliasID, aliasType, nameFormID, name));
				return std::string(name);
			}
		}
		LogAliasOnce(a_quest->formID, a_tokenName, std::format(
			"aliasID={} type={} forced={} nameFormID={:08X} locMapSize={} instances={} -> MISS",
			aliasID, aliasType, forcedLoc != nullptr, nameFormID,
			a_quest->aliasedLocMap.size(), a_quest->instanceDataArray.size()));
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
			// Radiant quests carry raw "<alias=...>" placeholders in fullName;
			// substitute from the quest's alias/instance data (member reads +
			// one proven engine call). Unresolvable tokens stay verbatim.
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
