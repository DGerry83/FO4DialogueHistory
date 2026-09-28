#include "CosaveCodec.h"

#include <string>
#include <utility>

namespace
{
	static_assert(std::to_underlying(F4DH::Core::SpeakerKind::Player) == 0 &&
				  std::to_underlying(F4DH::Core::SpeakerKind::Npc) == 1 &&
				  std::to_underlying(F4DH::Core::SpeakerKind::Unknown) == 2);

	void WriteU8(std::vector<std::byte>& a_out, std::uint8_t a_value)
	{
		a_out.push_back(std::byte{ a_value });
	}

	void WriteU32(std::vector<std::byte>& a_out, std::uint32_t a_value)
	{
		for (unsigned shift = 0; shift < 32; shift += 8) {
			a_out.push_back(static_cast<std::byte>((a_value >> shift) & 0xFF));
		}
	}

	void WriteString(std::vector<std::byte>& a_out, const std::string& a_value)
	{
		WriteU32(a_out, static_cast<std::uint32_t>(a_value.size()));
		a_out.insert(a_out.end(), reinterpret_cast<const std::byte*>(a_value.data()),
			reinterpret_cast<const std::byte*>(a_value.data() + a_value.size()));
	}

	// Cursor reader: every accessor bounds-checks and fails closed, so a
	// truncated or garbage payload can never read past the span.
	class Reader
	{
	public:
		explicit Reader(std::span<const std::byte> a_data) :
			_data(a_data)
		{}

		[[nodiscard]] bool U8(std::uint8_t& a_out)
		{
			if (_pos + 1 > _data.size()) {
				return false;
			}
			a_out = static_cast<std::uint8_t>(_data[_pos]);
			_pos += 1;
			return true;
		}

		[[nodiscard]] bool U32(std::uint32_t& a_out)
		{
			if (_pos + 4 > _data.size()) {
				return false;
			}
			std::uint32_t value = 0;
			for (unsigned shift = 0; shift < 32; shift += 8) {
				value |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(_data[_pos])) << shift;
				_pos += 1;
			}
			a_out = value;
			return true;
		}

		[[nodiscard]] bool String(std::string& a_out)
		{
			std::uint32_t length = 0;
			if (!U32(length)) {
				return false;
			}
			if (length > _data.size() - _pos) {  // garbage length field
				return false;
			}
			if (length == 0) {
				a_out.clear();
				return true;
			}
			a_out.assign(reinterpret_cast<const char*>(_data.data() + _pos), length);
			_pos += length;
			return true;
		}

		[[nodiscard]] bool AtEnd() const { return _pos == _data.size(); }

	private:
		std::span<const std::byte> _data;
		std::size_t                _pos{ 0 };
	};

	[[nodiscard]] bool ByteToKind(std::uint8_t a_byte, F4DH::Core::SpeakerKind& a_out)
	{
		switch (a_byte) {
		case 0: a_out = F4DH::Core::SpeakerKind::Player; return true;
		case 1: a_out = F4DH::Core::SpeakerKind::Npc; return true;
		case 2: a_out = F4DH::Core::SpeakerKind::Unknown; return true;
		default: return false;
		}
	}
}

namespace F4DH::Core
{
	std::vector<std::byte> CosaveCodec::Encode(const std::vector<DialogueLine>& a_lines)
	{
		std::vector<std::byte> out;
		out.reserve(4 + a_lines.size() * 32);
		WriteU32(out, static_cast<std::uint32_t>(a_lines.size()));
		for (const auto& line : a_lines) {
			WriteU8(out, static_cast<std::uint8_t>(line.kind));
			WriteU32(out, line.questId);
			WriteString(out, line.speaker);
			WriteString(out, line.text);
			WriteString(out, line.questName);
			WriteU8(out, line.questType);  // v2 layout
		}
		return out;
	}

	bool CosaveCodec::Decode(std::span<const std::byte> a_data, std::vector<DialogueLine>& a_outLines, std::uint32_t a_version)
	{
		a_outLines.clear();

		if (a_version < 1 || a_version > 2) {
			return false;  // unknown layout — fail closed
		}

		Reader reader(a_data);
		std::uint32_t count = 0;
		if (!reader.U32(count)) {
			return false;
		}

		for (std::uint32_t i = 0; i < count; ++i) {
			DialogueLine  line;
			std::uint8_t  kind = 0;
			if (!reader.U8(kind) || !ByteToKind(kind, line.kind)) {
				return false;
			}
			if (!reader.U32(line.questId)) {
				return false;
			}
			if (!reader.String(line.speaker) || !reader.String(line.text) || !reader.String(line.questName)) {
				return false;
			}
			if (a_version == 2) {
				if (!reader.U8(line.questType)) {
					return false;
				}
			}  // v1: questType stays 0 (kNone)
			a_outLines.push_back(std::move(line));
		}

		// A well-formed payload is consumed exactly; trailing bytes mean the
		// record is not a buffer payload of the claimed version.
		return reader.AtEnd();
	}
}
