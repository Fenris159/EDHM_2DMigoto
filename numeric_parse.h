#pragma once

#include <cerrno>
#include <cstdlib>
#include <limits>

inline int parse_int_or_zero(const char *text) noexcept
{
	if (!text)
		return 0;

	errno = 0;
	char *end = nullptr;
	const long value = std::strtol(text, &end, 10);
	if (end == text || errno == ERANGE || value < (std::numeric_limits<int>::min)() ||
	    value > (std::numeric_limits<int>::max)())
		return 0;

	return static_cast<int>(value);
}

inline double parse_double_or_zero(const char *text) noexcept
{
	if (!text)
		return 0.0;

	errno = 0;
	char *end = nullptr;
	const double value = std::strtod(text, &end);
	return end == text || errno == ERANGE ? 0.0 : value;
}
