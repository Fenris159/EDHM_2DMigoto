#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <stdexcept>

namespace
{
using Microsoft::WRL::ComPtr;

void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

template <typename T> T exported(HMODULE module, const char *name)
{
	FARPROC address = GetProcAddress(module, name);
	T result = nullptr;
	static_assert(sizeof(result) == sizeof(address));
	std::memcpy(&result, &address, sizeof(result));
	require(result != nullptr, "required fixture export is missing");
	return result;
}

HMODULE load_system(const wchar_t *name)
{
	wchar_t path[MAX_PATH] = {};
	require(GetSystemDirectoryW(path, MAX_PATH) != 0, "system directory unavailable");
	require(wcscat_s(path, L"\\") == 0 && wcscat_s(path, name) == 0, "system path overflow");
	HMODULE module = LoadLibraryW(path);
	require(module != nullptr, "could not load system graphics module");
	return module;
}

void check_device(ID3D11Device *device, UINT wanted)
{
	require(device != nullptr, "creation returned no device");
	constexpr UINT mask = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
	require((device->GetCreationFlags() & mask) == wanted, "proxy device policy was bypassed");
	D3D11_BUFFER_DESC description = {};
	description.ByteWidth = 64;
	description.Usage = D3D11_USAGE_DEFAULT;
	description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ComPtr<ID3D11Buffer> buffer;
	require(SUCCEEDED(device->CreateBuffer(&description, nullptr, &buffer)), "wrapped device cannot create buffer");
	D3D11_BUFFER_DESC actual = {};
	buffer->GetDesc(&actual);
	require(actual.ByteWidth == 64, "wrapped resource descriptor changed");
}

void run(const char *mode)
{
	load_system(L"dxgi.dll");
	wchar_t path[MAX_PATH] = {};
	require(GetModuleFileNameW(nullptr, path, MAX_PATH) != 0, "executable path unavailable");
	wchar_t *leaf = wcsrchr(path, L'\\');
	require(leaf != nullptr, "executable directory unavailable");
	*(leaf + 1) = L'\0';
	require(wcscat_s(path, L"d3d11.dll") == 0, "proxy path overflow");
	HMODULE proxy = LoadLibraryW(path);
	require(proxy != nullptr, "could not load game-local proxy");
	const bool chain = std::strncmp(mode, "chain-", 6) == 0;
	const bool downstream = std::strcmp(mode, "downstream") == 0;
	if (std::strcmp(mode, "chain-early") == 0 || std::strcmp(mode, "chain-swap-early") == 0)
	{
		auto initialize = exported<BOOL(WINAPI *)()>(proxy, "ChainInitialize");
		require(initialize() != FALSE, "early chain initialization failed");
	}
	auto create = exported<PFN_D3D11_CREATE_DEVICE>(proxy, "D3D11CreateDevice");
	auto swap = exported<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(proxy, "D3D11CreateDeviceAndSwapChain");
	const bool edup = std::strcmp(mode, "edup") == 0;
	if (edup)
	{
		// A successful capability-only probe does not give EDUP a device to
		// attach to. The later real creation must still reach its exports.
		D3D_FEATURE_LEVEL level = {};
		require(SUCCEEDED(create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, nullptr,
		                         &level, nullptr)),
		        "capability-only probe failed");
	}
	const UINT wanted = chain ? D3D11_CREATE_DEVICE_BGRA_SUPPORT : downstream ? D3D11_CREATE_DEVICE_SINGLETHREADED : 0;
	const UINT buffers = chain ? 2 : downstream ? 3 : 1;
	const bool swap_first = std::strcmp(mode, "chain-swap-early") == 0;
	WNDCLASSW window_class = {};
	window_class.lpfnWndProc = DefWindowProcW;
	window_class.hInstance = GetModuleHandleW(nullptr);
	window_class.lpszClassName = L"EdhmProxyChainFixture";
	require(RegisterClassW(&window_class) != 0, "window registration failed");
	HWND window = CreateWindowW(window_class.lpszClassName, L"EDHM off-game proxy check", WS_OVERLAPPEDWINDOW, 0, 0, 64,
	                            64, nullptr, nullptr, window_class.hInstance, nullptr);
	require(window != nullptr, "hidden test window unavailable");
	for (unsigned pass = 0; pass < 4; ++pass)
	{
		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> context;
		ComPtr<IDXGISwapChain> swap_chain;
		const bool with_swap = ((pass % 2) != 0) != swap_first;
		HRESULT hr;
		if (with_swap)
		{
			DXGI_SWAP_CHAIN_DESC description = {};
			description.BufferDesc.Width = 16;
			description.BufferDesc.Height = 16;
			description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			description.SampleDesc.Count = 1;
			description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			description.BufferCount = 1;
			description.OutputWindow = window;
			description.Windowed = TRUE;
			description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
			hr = swap(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &description,
			          &swap_chain, &device, nullptr, &context);
			require(SUCCEEDED(hr), "device and swap-chain creation failed");
			require(swap_chain != nullptr, "creation returned no swap chain");
			DXGI_SWAP_CHAIN_DESC actual = {};
			require(SUCCEEDED(swap_chain->GetDesc(&actual)), "swap-chain description unavailable");
			require(actual.BufferCount == buffers, "proxy swap-chain policy was bypassed");
		}
		else
		{
			hr = create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr,
			            &context);
			require(SUCCEEDED(hr), "device creation failed");
		}
		check_device(device.Get(), wanted);
		require(context != nullptr, "creation returned no context");
		context->ClearState();
		if (edup && swap_chain)
			require(SUCCEEDED(swap_chain->Present(0, 0)), "EDUP presentation failed");
		std::printf("PASS %s creation %u (%s)\n", mode, pass + 1, with_swap ? "device+swap" : "device");
	}
	DestroyWindow(window);
}
} // namespace

int main(int argc, char **argv)
{
	try
	{
		require(argc == 2, "expected one fixture mode");
		run(argv[1]);
		return 0;
	}
	catch (const std::exception &error)
	{
		std::fprintf(stderr, "FAIL: %s\n", error.what());
		return 1;
	}
}
