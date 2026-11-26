#pragma once

#include <cstddef>
#include <cstdint>
#include <d3d11_1.h>

struct TypedUavLoadInfo
{
	bool requiresAdditionalFormats = false;
	uint64_t typedLoadUavMask = 0;
};

struct TypedUavLoadShader
{
	uint64_t mask = 0;
	uint64_t hash = 0;
};

// Invalid containers return an empty result. Unknown UAV indexing checks all slots.
TypedUavLoadInfo InspectTypedUavLoads(const void *bytecode, size_t length) noexcept;
bool IsAlwaysSupportedTypedUavLoadFormat(DXGI_FORMAT format) noexcept;
bool IsConcreteTypedUav(const D3D11_UNORDERED_ACCESS_VIEW_DESC &desc) noexcept;
