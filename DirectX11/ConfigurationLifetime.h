#pragma once

#include <mutex>
#include <optional>
#include <shared_mutex>

// Hold a read lease before borrowing configuration objects, through their last
// use (including post commands). Reload owns the write lease through save,
// parse and initialization. Nested command execution on one thread reuses its
// outer lease; commands initialized by a writer do not reacquire the mutex.
namespace ConfigurationLifetime
{
inline std::shared_mutex mutex;
inline thread_local unsigned readers = 0;
inline thread_local unsigned writers = 0;
}

class ConfigurationReadLock
{
	std::optional<std::shared_lock<std::shared_mutex>> lock;

  public:
	ConfigurationReadLock()
	{
		if (!ConfigurationLifetime::readers && !ConfigurationLifetime::writers)
			lock.emplace(ConfigurationLifetime::mutex);
		++ConfigurationLifetime::readers;
	}
	~ConfigurationReadLock()
	{
		--ConfigurationLifetime::readers;
	}
	ConfigurationReadLock(const ConfigurationReadLock &) = delete;
	ConfigurationReadLock &operator=(const ConfigurationReadLock &) = delete;
	ConfigurationReadLock(ConfigurationReadLock &&) = delete;
	ConfigurationReadLock &operator=(ConfigurationReadLock &&) = delete;
};

class ConfigurationWriteLock
{
	std::optional<std::unique_lock<std::shared_mutex>> lock;
	bool acquired = false;

	explicit ConfigurationWriteLock(bool try_only)
	{
		// Replacing objects from within a reader would invalidate its own
		// command stack. The caller must defer that request to a frame boundary.
		if (ConfigurationLifetime::readers)
			return;
		if (!ConfigurationLifetime::writers)
		{
			lock.emplace(ConfigurationLifetime::mutex, std::defer_lock);
			if (try_only)
			{
				if (!lock->try_lock())
					return;
			}
			else
				lock->lock();
		}
		++ConfigurationLifetime::writers;
		acquired = true;
	}

  public:
	ConfigurationWriteLock() : ConfigurationWriteLock(false) {}
	explicit ConfigurationWriteLock(std::try_to_lock_t) : ConfigurationWriteLock(true) {}
	~ConfigurationWriteLock()
	{
		if (acquired)
			--ConfigurationLifetime::writers;
	}
	bool owns_lock() const
	{
		return acquired;
	}
	ConfigurationWriteLock(const ConfigurationWriteLock &) = delete;
	ConfigurationWriteLock &operator=(const ConfigurationWriteLock &) = delete;
	ConfigurationWriteLock(ConfigurationWriteLock &&) = delete;
	ConfigurationWriteLock &operator=(ConfigurationWriteLock &&) = delete;
};
