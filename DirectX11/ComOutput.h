#pragma once

#include <winerror.h>

// COM out parameters must be null on failure. Release a non-conforming
// callee's returned object before restoring that contract.
template <typename Interface>
HRESULT NormalizeComFailureOutput(HRESULT result, Interface **output) noexcept
{
	if (FAILED(result) && output && *output) {
		(*output)->Release();
		*output = nullptr;
	}
	return result;
}

// A required COM output must contain an interface on success. Convert a
// non-conforming success into a controlled failure rather than returning a
// null object to a caller that is entitled to dereference it.
template <typename Interface>
HRESULT NormalizeRequiredComOutput(HRESULT result, Interface **output) noexcept
{
	result = NormalizeComFailureOutput(result, output);
	if (!output)
		return E_POINTER;
	if (SUCCEEDED(result) && !*output)
		return E_UNEXPECTED;
	return result;
}
