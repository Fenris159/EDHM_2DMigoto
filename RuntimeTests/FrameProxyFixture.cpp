// Model a proxy that patches the game's Present and copies its buffer instead
// of calling the saved Present. This DLL is the third-party API boundary.
#include <windows.h>
#include <dxgi1_2.h>

#include <cstring>

namespace
{
using Present = HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain *, UINT, UINT);
using Present1 = HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain1 *, UINT, UINT, const DXGI_PRESENT_PARAMETERS *);
Present original;
Present1 original1;
unsigned mode;

HRESULT read_buffer(IDXGISwapChain *chain)
{
	IUnknown *buffer = nullptr;
	HRESULT hr = chain->GetBuffer(0, IID_PPV_ARGS(&buffer));
	if (buffer)
		buffer->Release();
	return hr;
}

HRESULT STDMETHODCALLTYPE present(IDXGISwapChain *chain, UINT interval, UINT flags)
{
	if (flags & DXGI_PRESENT_TEST)
		return S_OK;
	HRESULT hr = read_buffer(chain);
	if (SUCCEEDED(hr))
		hr = read_buffer(chain); // Copies/queries in one frame must not tick twice.
	if (FAILED(hr))
		return hr;
	if (mode == 1)
		return original(chain, interval, flags);
	return mode == 2 ? E_FAIL : S_OK;
}

HRESULT STDMETHODCALLTYPE present1(IDXGISwapChain1 *chain, UINT interval, UINT flags,
                                   const DXGI_PRESENT_PARAMETERS *parameters)
{
	if (flags & DXGI_PRESENT_TEST)
		return S_OK;
	HRESULT hr = read_buffer(chain);
	if (SUCCEEDED(hr))
		hr = read_buffer(chain);
	if (FAILED(hr))
		return hr;
	if (mode == 1)
		return original1(chain, interval, flags, parameters);
	return mode == 2 ? E_FAIL : S_OK;
}
} // namespace

extern "C" __declspec(dllexport) BOOL WINAPI InstallFrameProxy(IDXGISwapChain *chain)
{
	auto **table = *reinterpret_cast<void ***>(chain);
	DWORD protection = 0;
	if (!VirtualProtect(table, sizeof(void *) * 23, PAGE_READWRITE, &protection))
		return FALSE;
	std::memcpy(&original, table + 8, sizeof(original));
	std::memcpy(&original1, table + 22, sizeof(original1));
	auto replacement = &present;
	auto replacement1 = &present1;
	std::memcpy(table + 8, &replacement, sizeof(replacement));
	std::memcpy(table + 22, &replacement1, sizeof(replacement1));
	DWORD ignored = 0;
	return VirtualProtect(table, sizeof(void *) * 23, protection, &ignored);
}

extern "C" __declspec(dllexport) void WINAPI SetFrameProxyMode(unsigned value)
{
	mode = value;
}

extern "C" __declspec(dllexport) HRESULT WINAPI ReadFrameProxyBuffer(IDXGISwapChain *chain)
{
	return read_buffer(chain);
}

extern "C" __declspec(dllexport) BOOL WINAPI RemoveFrameProxy(IDXGISwapChain *chain)
{
	auto **table = *reinterpret_cast<void ***>(chain);
	DWORD protection = 0;
	if (!VirtualProtect(table, sizeof(void *) * 23, PAGE_READWRITE, &protection))
		return FALSE;
	std::memcpy(table + 8, &original, sizeof(original));
	std::memcpy(table + 22, &original1, sizeof(original1));
	DWORD ignored = 0;
	return VirtualProtect(table, sizeof(void *) * 23, protection, &ignored);
}
