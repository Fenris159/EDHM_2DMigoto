#pragma once

#include <d3d11.h>

#include <cstring>
#include <exception>
#include <stdexcept>

// A quiescent, single-threaded fixture replaces only the external Map slot.
// All other D3D work, including readback, uses the real context. Do not use
// this fixture concurrently with another Map interception in the same process.
class D3DMapBoundary
{
	using Map = HRESULT(STDMETHODCALLTYPE *)(ID3D11DeviceContext *, ID3D11Resource *, UINT, D3D11_MAP, UINT,
	                                         D3D11_MAPPED_SUBRESOURCE *);
	inline static D3DMapBoundary *active = nullptr;
	ID3D11DeviceContext *context;
	ID3D11Resource *resource;
	void **slot;
	Map original = nullptr;

	static HRESULT STDMETHODCALLTYPE map(ID3D11DeviceContext *context, ID3D11Resource *resource, UINT subresource,
	                                     D3D11_MAP kind, UINT flags, D3D11_MAPPED_SUBRESOURCE *mapped)
	{
		auto &boundary = *active;
		if (context == boundary.context && resource == boundary.resource && kind == D3D11_MAP_WRITE_DISCARD &&
		    boundary.failures)
		{
			--boundary.failures;
			mapped->pData = nullptr;
			return E_OUTOFMEMORY;
		}
		return boundary.original(context, resource, subresource, kind, flags, mapped);
	}

  public:
	unsigned failures = 0;

	D3DMapBoundary(ID3D11DeviceContext *target_context, ID3D11Resource *target_resource)
	    : context(target_context), resource(target_resource), slot(*reinterpret_cast<void ***>(context) + 14)
	{
		if (active)
			throw std::runtime_error("a Map boundary is already active");
		static_assert(sizeof(original) == sizeof(*slot));
		std::memcpy(&original, slot, sizeof(original));
		DWORD protection = 0;
		if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection))
			throw std::runtime_error("Map slot is not writable");
		Map replacement = &map;
		std::memcpy(slot, &replacement, sizeof(replacement));
		DWORD ignored = 0;
		if (!VirtualProtect(slot, sizeof(*slot), protection, &ignored))
		{
			std::memcpy(slot, &original, sizeof(original));
			VirtualProtect(slot, sizeof(*slot), protection, &ignored);
			throw std::runtime_error("Map protection restoration failed");
		}
		active = this;
	}

	D3DMapBoundary(const D3DMapBoundary &) = delete;
	D3DMapBoundary &operator=(const D3DMapBoundary &) = delete;
	D3DMapBoundary(D3DMapBoundary &&) = delete;
	D3DMapBoundary &operator=(D3DMapBoundary &&) = delete;

	~D3DMapBoundary()
	{
		DWORD protection = 0;
		if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection))
			std::terminate();
		std::memcpy(slot, &original, sizeof(original));
		DWORD ignored = 0;
		if (!VirtualProtect(slot, sizeof(*slot), protection, &ignored))
			std::terminate();
		active = nullptr;
	}
};
