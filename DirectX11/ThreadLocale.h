#pragma once

#include <clocale>
#include <string>

// setlocale is process-global by default in the MSVC CRT. Switch only the
// current thread while parsing or formatting configuration data, then restore
// both the locale and the thread-locale mode on scope exit.
class ScopedThreadLocale
{
public:
	explicit ScopedThreadLocale(int category, const char *locale) :
		category_(category),
		previous_mode_(_configthreadlocale(_ENABLE_PER_THREAD_LOCALE)),
		changed_(false)
	{
		const char *current = setlocale(category_, nullptr);
		if (current)
			previous_locale_ = current;
		changed_ = setlocale(category_, locale) != nullptr;
	}

	~ScopedThreadLocale()
	{
		if (changed_ && !previous_locale_.empty())
			setlocale(category_, previous_locale_.c_str());
		if (previous_mode_ != -1)
			_configthreadlocale(previous_mode_);
	}

	ScopedThreadLocale(const ScopedThreadLocale&) = delete;
	ScopedThreadLocale& operator=(const ScopedThreadLocale&) = delete;

	bool changed() const { return changed_; }

private:
	int category_;
	int previous_mode_;
	bool changed_;
	std::string previous_locale_;
};
