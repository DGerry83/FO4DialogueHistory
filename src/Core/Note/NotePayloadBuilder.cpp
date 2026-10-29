#include "NotePayloadBuilder.h"

#include <format>

namespace
{
	const char* StatusName(F4DH::Core::NoteBodyResult::Status a_status)
	{
		switch (a_status) {
		case F4DH::Core::NoteBodyResult::Status::Ready: return "ready";
		case F4DH::Core::NoteBodyResult::Status::MissingPlugin: return "missingPlugin";
		case F4DH::Core::NoteBodyResult::Status::Empty: return "empty";
		default: return "unavailable";
		}
	}
}

namespace F4DH::Core
{
	std::string NotePayloadBuilder::BuildList(const std::vector<NoteListItem>& a_items)
	{
		std::string json = "[";
		for (std::size_t i = 0; i < a_items.size(); ++i) {
			if (i != 0) json += ',';
			const auto& item = a_items[i];
			json += std::format("{{\"id\":{},\"title\":\"{}\",\"sequence\":{}}}",
				item.entryId, Escape(item.title), item.firstReadSequence);
		}
		json += ']';
		return json;
	}

	std::string NotePayloadBuilder::BuildTree(const std::vector<NoteFolder>& a_folders, const std::vector<NoteListItem>& a_items)
	{
		std::string json = "{\"folders\":[";
		for (std::size_t i = 0; i < a_folders.size(); ++i) {
			if (i != 0) json += ',';
			const auto& folder = a_folders[i];
			json += std::format("{{\"id\":{},\"parentId\":{},\"name\":\"{}\",\"ordinal\":{}}}",
				folder.folderId, folder.parentFolderId, Escape(folder.name), folder.ordinal);
		}
		json += "],\"notes\":[";
		for (std::size_t i = 0; i < a_items.size(); ++i) {
			if (i != 0) json += ',';
			const auto& item = a_items[i];
			json += std::format("{{\"id\":{},\"folderId\":{},\"title\":\"{}\",\"sequence\":{}}}",
				item.entryId, item.folderId, Escape(item.title), item.firstReadSequence);
		}
		json += "]}";
		return json;
	}

	std::string NotePayloadBuilder::BuildOperationResult(const NoteOperation& a_result)
	{
		return std::format("{{\"ok\":{},\"id\":{},\"message\":\"{}\"}}",
			a_result.ok ? "true" : "false", a_result.id, Escape(a_result.message));
	}

	std::string NotePayloadBuilder::BuildBody(const NoteBodyResult& a_result)
	{
		std::string encounter = "null";
		if (a_result.firstEncounter) {
			const auto& first = *a_result.firstEncounter;
			const auto& time = first.timestamp;
			encounter = std::format(
				"{{\"kind\":\"{}\",\"year\":{},\"month\":{},\"day\":{},\"hour\":{},\"minute\":{},\"location\":\"{}\"}}",
				first.kind == EncounterKind::Pickup ? "pickup" : "read",
				time.year, time.month, time.day, time.hour, time.minute, Escape(first.location));
		}
		return std::format(
			"{{\"id\":{},\"status\":\"{}\",\"title\":\"{}\",\"body\":\"{}\",\"plugin\":\"{}\",\"firstEncounter\":{}}}",
			a_result.entryId, StatusName(a_result.status), Escape(a_result.title), Escape(a_result.body),
			Escape(a_result.pluginName), encounter);
	}

	std::string NotePayloadBuilder::Escape(std::string_view a_value)
	{
		std::string out;
		out.reserve(a_value.size() + 8);
		for (const char c : a_value) {
			switch (c) {
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (static_cast<unsigned char>(c) < 0x20) {
					constexpr char hex[] = "0123456789abcdef";
					out += "\\u00";
					out += hex[(c >> 4) & 0xF];
					out += hex[c & 0xF];
				} else {
					out += c;
				}
			}
		}
		return out;
	}
}
