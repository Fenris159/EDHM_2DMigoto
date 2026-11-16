#pragma once

// One request belongs to one context. A successful deferred-context upload
// records work there; it must not acknowledge another context's request.
class IniParamsPublication
{
	bool pending = false;

  public:
	// The upload consumes the current complete CPU image synchronously. Failure
	// retains the request, not a pointer or snapshot that can become stale.
	template <class Upload> bool Publish(bool changed, Upload &&upload)
	{
		pending |= changed;
		if (!pending)
			return true;
		if (!upload())
			return false;
		pending = false;
		return true;
	}
};
