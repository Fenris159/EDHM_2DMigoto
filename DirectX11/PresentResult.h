#pragma once

#include <dxgi.h>

static inline bool IsDeviceLossPresentResult(HRESULT result)
{
	return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET ||
		result == DXGI_ERROR_DEVICE_HUNG || result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}
