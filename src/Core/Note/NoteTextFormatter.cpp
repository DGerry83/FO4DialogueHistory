#include "NoteTextFormatter.h"

#include <algorithm>
#include <cctype>

namespace
{
	void AppendNewline(std::string& a_out, bool a_paragraph = false)
	{
		const auto nonWhitespace = a_out.find_last_not_of(" \t\n");
		if (nonWhitespace == std::string::npos) {
			return;
		}
		a_out.erase(nonWhitespace + 1);
		std::size_t newlines = 0;
		for (auto it = a_out.rbegin(); it != a_out.rend() && *it == '\n'; ++it) ++newlines;
		const auto wanted = a_paragraph ? 2u : 1u;
		for (auto count = newlines; count < wanted; ++count) {
			a_out.push_back('\n');
		}
	}

	void AppendDecodedEntity(std::string_view a_entity, std::string& a_out)
	{
		if (a_entity == "lt") a_out += '<';
		else if (a_entity == "gt") a_out += '>';
		else if (a_entity == "amp") a_out += '&';
		else if (a_entity == "quot") a_out += '"';
		else if (a_entity == "apos") a_out += '\'';
		else {
			a_out += '&';
			a_out += a_entity;
			a_out += ';';
		}
	}

	std::string TrimAndCollapse(std::string_view a_input)
	{
		std::string out;
		out.reserve(a_input.size());
		bool pendingSpace = false;
		for (const unsigned char c : a_input) {
			if (std::isspace(c)) {
				pendingSpace = !out.empty();
				continue;
			}
			if (pendingSpace) {
				out.push_back(' ');
				pendingSpace = false;
			}
			out.push_back(static_cast<char>(c));
		}
		return out;
	}
}

namespace F4DH::Core
{
	std::string NoteTextFormatter::ToPlainText(std::string_view a_text)
	{
		std::string out;
		out.reserve(a_text.size());
		for (std::size_t i = 0; i < a_text.size();) {
			if (a_text[i] == '\r') {
				if (i + 1 < a_text.size() && a_text[i + 1] == '\n') ++i;
				out.push_back('\n');
				++i;
				continue;
			}
			if (a_text[i] == '\n') {
				out.push_back('\n');
				++i;
				continue;
			}
			if (a_text[i] == '\t') {
				out.push_back(' ');
				++i;
				continue;
			}
			if (a_text[i] == '&') {
				const auto end = a_text.find(';', i + 1);
				if (end != std::string_view::npos && end - i <= 8) {
					AppendDecodedEntity(a_text.substr(i + 1, end - i - 1), out);
					i = end + 1;
					continue;
				}
			}
			if (a_text[i] == '[' && a_text.substr(i, 11) == "[pagebreak]") {
				out += "\n\n--- Page Break ---\n\n";
				i += 11;
				continue;
			}
			if (a_text[i] == '<') {
				const auto end = a_text.find('>', i + 1);
				if (end == std::string_view::npos) {
					out.push_back(a_text[i++]);
					continue;
				}
				const auto tag = a_text.substr(i + 1, end - i - 1);
				if (tag.starts_with("img")) {
					out += "[Image omitted]";
				} else if (tag.starts_with("/p")) {
					out += "\n\n";
				} else if (tag.starts_with("br") || tag.starts_with("p")) {
					out.push_back('\n');
				}
				i = end + 1;
				continue;
			}
			out.push_back(a_text[i++]);
		}

		std::string lines;
		lines.reserve(out.size());
		bool blankLine = false;
		std::size_t start = 0;
		while (start <= out.size()) {
			const auto end = out.find('\n', start);
			const auto line = TrimAndCollapse(std::string_view(out).substr(start, end - start));
			if (line.empty()) {
				blankLine = !lines.empty();
			} else {
				if (!lines.empty()) lines += blankLine ? "\n\n" : "\n";
				lines += line;
				blankLine = false;
			}
			if (end == std::string::npos) break;
			start = end + 1;
		}
		return lines;
	}

	std::string NoteTextFormatter::NormalizeOpening(std::string_view a_text)
	{
		return TrimAndCollapse(a_text);
	}

	bool NoteTextFormatter::ContainsUnresolvedAlias(std::string_view a_text) noexcept
	{
		for (std::size_t i = 0; i + 6 < a_text.size(); ++i) {
			if (a_text[i] == '<' &&
				(a_text.substr(i + 1, 6) == "Alias=" || a_text.substr(i + 1, 6) == "Alias.")) {
				return true;
			}
		}
		return false;
	}
}
