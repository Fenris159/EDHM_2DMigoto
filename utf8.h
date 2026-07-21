#pragma once

#include <Windows.h>

#include <limits>
#include <exception>
#include <string>

static inline bool utf8_to_wide(const char *input, size_t input_size, std::wstring *output)
{
	if (!output || input_size > static_cast<size_t>((std::numeric_limits<int>::max)()) ||
			(!input && input_size))
		return false;
	output->clear();
	if (!input_size)
		return true;

	int input_length = static_cast<int>(input_size);
	int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, input_length, nullptr, 0);
	if (!length)
		return false;
	try {
		output->resize(length);
	} catch (const std::exception&) {
		return false;
	}
	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, input_length, &(*output)[0], length)) {
		output->clear();
		return false;
	}
	return true;
}

static inline bool utf8_to_wide(const std::string &input, std::wstring *output)
{
	return utf8_to_wide(input.data(), input.size(), output);
}

static inline bool wide_to_utf8(const wchar_t *input, size_t input_size, std::string *output)
{
	if (!output || input_size > static_cast<size_t>((std::numeric_limits<int>::max)()) ||
			(!input && input_size))
		return false;
	output->clear();
	if (!input_size)
		return true;

	int input_length = static_cast<int>(input_size);
	int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input, input_length, nullptr, 0, nullptr, nullptr);
	if (!length)
		return false;
	try {
		output->resize(length);
	} catch (const std::exception&) {
		return false;
	}
	if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input, input_length, &(*output)[0], length, nullptr, nullptr)) {
		output->clear();
		return false;
	}
	return true;
}

static inline bool wide_to_utf8(const std::wstring &input, std::string *output)
{
	return wide_to_utf8(input.data(), input.size(), output);
}
