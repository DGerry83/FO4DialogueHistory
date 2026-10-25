#include "MessageCosaveCodec.h"

#include <string>
#include <utility>

namespace
{
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
}

namespace F4DH::Core
{
	std::vector<std::byte> MessageCosaveCodec::Encode(const std::vector<MessageLine>& a_messages)
	{
		std::vector<std::byte> out;
		out.reserve(4 + a_messages.size() * 16);
		WriteU32(out, static_cast<std::uint32_t>(a_messages.size()));
		for (const auto& message : a_messages) {
			WriteString(out, message.text);
		}
		return out;
	}

	bool MessageCosaveCodec::Decode(std::span<const std::byte> a_data, std::vector<MessageLine>& a_outMessages, std::uint32_t a_version)
	{
		a_outMessages.clear();

		if (a_version != 1) {
			return false;  // unknown layout — fail closed
		}

		Reader reader(a_data);
		std::uint32_t count = 0;
		if (!reader.U32(count)) {
			return false;
		}

		for (std::uint32_t i = 0; i < count; ++i) {
			MessageLine message;
			if (!reader.String(message.text)) {
				return false;
			}
			a_outMessages.push_back(std::move(message));
		}

		// A well-formed payload is consumed exactly; trailing bytes mean the
		// record is not a buffer payload of the claimed version.
		return reader.AtEnd();
	}
}
