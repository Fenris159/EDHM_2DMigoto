#pragma once

#include <Windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>

void InitD311();
void DestroyDLL();

extern "C" HMODULE(__stdcall *fnOrigLoadLibraryExW)(_In_ LPCWSTR lpLibFileName, _Reserved_ HANDLE hFile,
                                                    _In_ DWORD dwFlags);

extern "C" HMODULE __stdcall Hooked_LoadLibraryExW(_In_ LPCWSTR lpLibFileName, _Reserved_ HANDLE hFile,
                                                   _In_ DWORD dwFlags);

// These are moved up to the header file so we can call them from elsewhere.
// We can't include d3d11.lib as a linker connection, because we are reimplementing
// all those routines, and thus get duplicate symbols.

extern "C" PFN_D3D11_CREATE_DEVICE _D3D11CreateDevice;
extern "C" PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN _D3D11CreateDeviceAndSwapChain;

// Detours resume the module they intercepted; proxy exports select the backend.
// These routes must remain independent when an outer and downstream proxy coexist.
extern "C" PFN_D3D11_CREATE_DEVICE fnOrigD3D11CreateDevice;
extern "C" PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN fnOrigD3D11CreateDeviceAndSwapChain;

extern "C" HRESULT WINAPI Hooked_D3D11CreateDevice(IDXGIAdapter *adapter, D3D_DRIVER_TYPE driverType, HMODULE software,
                                                   UINT flags, const D3D_FEATURE_LEVEL *featureLevels, UINT count,
                                                   UINT sdkVersion, ID3D11Device **device,
                                                   D3D_FEATURE_LEVEL *featureLevel, ID3D11DeviceContext **context);
extern "C" HRESULT WINAPI Hooked_D3D11CreateDeviceAndSwapChain(
    IDXGIAdapter *adapter, D3D_DRIVER_TYPE driverType, HMODULE software, UINT flags,
    const D3D_FEATURE_LEVEL *featureLevels, UINT count, UINT sdkVersion, DXGI_SWAP_CHAIN_DESC *description,
    IDXGISwapChain **swapChain, ID3D11Device **device, D3D_FEATURE_LEVEL *featureLevel, ID3D11DeviceContext **context);

#ifdef NTDDI_WIN10
// 11On12 requires Win 10 SDK
#include <d3d11on12.h>

extern "C" PFN_D3D11ON12_CREATE_DEVICE _D3D11On12CreateDevice;
#endif
