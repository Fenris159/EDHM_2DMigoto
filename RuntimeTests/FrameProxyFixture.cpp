// Model a proxy that patches the game's Present and copies its buffer instead
// of calling the saved Present. This DLL is the third-party API boundary.
#include <windows.h>
#include <dxgi1_2.h>

#include <cstring>

namespace
{
using Present = HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain *, UINT, UINT);
Present original;
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
} // namespace

extern "C" __declspec(dllexport) BOOL WINAPI InstallFrameProxy(IDXGISwapChain *chain)
{
	auto **table = *reinterpret_cast<void ***>(chain);
	DWORD protection = 0;
	if (!VirtualProtect(table + 8, sizeof(void *), PAGE_READWRITE, &protection))
		return FALSE;
	std::memcpy(&original, table + 8, sizeof(original));
	auto replacement = &present;
	std::memcpy(table + 8, &replacement, sizeof(replacement));
	DWORD ignored = 0;
	return VirtualProtect(table + 8, sizeof(void *), protection, &ignored);
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
	if (!VirtualProtect(table + 8, sizeof(void *), PAGE_READWRITE, &protection))
		return FALSE;
	std::memcpy(table + 8, &original, sizeof(original));
	DWORD ignored = 0;
	return VirtualProtect(table + 8, sizeof(void *), protection, &ignored);
}
