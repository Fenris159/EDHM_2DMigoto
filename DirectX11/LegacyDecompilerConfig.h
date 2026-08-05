#pragma once

#include <cctype>
#include <string>
#include <vector>

#include "utf8.h"

static inline void TrimAscii(std::string *value)
{
	if (!value)
		return;
	size_t start = 0;
	while (start < value->size() && isspace(static_cast<unsigned char>((*value)[start])))
		++start;
	size_t end = value->size();
	while (end > start && isspace(static_cast<unsigned char>((*value)[end - 1])))
		--end;
	*value = value->substr(start, end - start);
}

static inline bool ParseDepthTextureSetting(const wchar_t *setting, std::string *texture, char *component)
{
	if (!setting || !texture || !component || !wide_to_utf8(setting, wcslen(setting), texture))
		return false;
	TrimAscii(texture);
	if (texture->size() < 3 || (*texture)[texture->size() - 2] != '.')
		return false;

	char parsed_component = texture->back();
	if (parsed_component != 'x' && parsed_component != 'y' &&
			parsed_component != 'z' && parsed_component != 'w')
		return false;
	texture->resize(texture->size() - 2);
	TrimAscii(texture);
	if (texture->empty())
		return false;
	*component = parsed_component;
	return true;
}

static inline bool ParseIdentifierList(const wchar_t *setting, std::vector<std::string> *values)
{
	if (!setting || !values)
		return false;

	std::string text;
	if (!wide_to_utf8(setting, wcslen(setting), &text))
		return false;

	std::vector<std::string> parsed;
	size_t position = 0;
	while (position < text.size()) {
		while (position < text.size() &&
				(text[position] == ',' || isspace(static_cast<unsigned char>(text[position]))))
			++position;
		if (position == text.size())
			break;

		size_t start = position;
		while (position < text.size() && text[position] != ',' &&
				!isspace(static_cast<unsigned char>(text[position])))
			++position;
		parsed.push_back(text.substr(start, position - start));
	}
	values->swap(parsed);
	return true;
}
