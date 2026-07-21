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
