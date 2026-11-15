#pragma once

#include <windows.h>
#include <vector>

// This version of EnterCriticalSection will use the function and line number
// in any lock stacks dumped when potential deadlock hazards are detected:
#define EnterCriticalSectionPretty(lock) _EnterCriticalSectionPretty(lock, __FUNCTION__, __LINE__)
void _EnterCriticalSectionPretty(CRITICAL_SECTION *lock, const char *function, int line);

class CriticalSectionLock
{
	CRITICAL_SECTION *section;

  public:
	explicit CriticalSectionLock(CRITICAL_SECTION *section) : section(section)
	{
		EnterCriticalSectionPretty(section);
	}
	~CriticalSectionLock()
	{
		LeaveCriticalSection(section);
	}
	CriticalSectionLock(const CriticalSectionLock &) = delete;
	CriticalSectionLock &operator=(const CriticalSectionLock &) = delete;
	CriticalSectionLock(CriticalSectionLock &&) = delete;
	CriticalSectionLock &operator=(CriticalSectionLock &&) = delete;
};

// Use this when initialising a critical section in 3DMigoto to give it a nice
// name in lock stack dumps rather than using its address.
//
// **AVOID CALLING THIS FROM GLOBAL CONSTRUCTORS**
// https://yosefk.com/c++fqa/ctors.html#fqa-10.12
#define InitializeCriticalSectionPretty(lock) _InitializeCriticalSectionPretty(lock, #lock)
void _InitializeCriticalSectionPretty(CRITICAL_SECTION *lock, const char *lock_name);

void enable_lock_dependency_checks();
struct held_lock_info
{
	CRITICAL_SECTION *lock;
	uintptr_t ret;
	size_t stack_hash;
	const char *function;
	int line;
};
typedef std::vector<held_lock_info> LockStack;
