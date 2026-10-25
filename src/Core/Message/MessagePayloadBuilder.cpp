#include "MessagePayloadBuilder.h"

namespace F4DH::Core
{
	std::string MessagePayloadBuilder::BuildSnapshot(const std::vector<MessageLine>& a_messages)
	{
		std::string json = "[";
		bool        first = true;
		for (const auto& message : a_messages) {
			if (!first) {
				json += ',';
			}
			first = false;
			json += "{\"text\":\"" + Escape(message.text) + "\"}";
		}
		json += ']';
		return json;
	}

	std::string MessagePayloadBuilder::BuildAppend(const MessageLine& a_message)
	{
		return "{\"text\":\"" + Escape(a_message.text) + "\"}";
	}

	std::string MessagePayloadBuilder::Escape(const std::string& value)
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
}
