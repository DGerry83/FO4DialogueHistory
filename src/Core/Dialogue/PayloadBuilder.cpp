#include "PayloadBuilder.h"

namespace F4DH::Core
{
	std::string PayloadBuilder::BuildSnapshot(const std::vector<DialogueLine>& lines)
	{
		std::string json = "[";
		bool        first = true;
		for (const auto& line : lines) {
			if (!first) {
				json += ',';
			}
			first = false;
			json += LineToJson(line);
		}
		json += ']';
		return json;
	}

	std::string PayloadBuilder::BuildAppend(const DialogueLine& line)
	{
		return LineToJson(line);
	}

	std::string PayloadBuilder::Escape(const std::string& value)
	{
		std::string out;
		out.reserve(value.size() + 8);
		for (const char c : value) {
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

	std::string PayloadBuilder::LineToJson(const DialogueLine& line)
	{
		const char* kind = "unknown";
		switch (line.kind) {
		case SpeakerKind::Player: kind = "player"; break;
		case SpeakerKind::Npc: kind = "npc"; break;
		case SpeakerKind::Unknown: break;
		}
		return "{\"speaker\":\"" + Escape(line.speaker) +
			"\",\"kind\":\"" + kind +
			"\",\"text\":\"" + Escape(line.text) +
			"\",\"questId\":" + std::to_string(line.questId) +
			",\"questName\":\"" + Escape(line.questName) + "\"}";
	}
}
