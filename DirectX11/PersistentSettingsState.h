#pragma once

#include <cstring>
#include <map>
#include <string>

// Keep temporarily unavailable settings across reloads without applying them to
// variables that are absent or no longer persistent. Compare complete values,
// rather than counts or a lossy hash, when deciding whether the grace expired.
class PersistentSettingsState
{
  public:
	using Values = std::map<std::wstring, float>;

	void BeginLoad(const std::wstring &path)
	{
		if (file != path)
			Clear();
		file = path;
		previous = values;
		values.clear();
		loading = true;
	}

	void Remember(const std::wstring &name, float value)
	{
		values[name] = value;
	}

	bool FinishLoad(bool clear_unknown)
	{
		if (!loading)
			return false;
		loading = false;
		bool unchanged = !values.empty() && values.size() == previous.size();
		if (unchanged)
		{
			auto old = previous.begin();
			for (const auto &entry : values)
			{
				if (entry.first != old->first || memcmp(&entry.second, &old->second, sizeof(float)) != 0)
				{
					unchanged = false;
					break;
				}
				++old;
			}
		}
		previous.clear();
		if (!clear_unknown || !unchanged)
			return false;
		values.clear();
		return true;
	}

	void Clear()
	{
		values.clear();
		previous.clear();
		file.clear();
		loading = false;
	}

	const Values &UnknownValues() const
	{
		return values;
	}

  private:
	std::wstring file;
	Values values;
	Values previous;
	bool loading = false;
};
