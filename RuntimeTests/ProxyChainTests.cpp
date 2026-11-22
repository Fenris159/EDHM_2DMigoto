#include <windows.h>
#include <d3d11_1.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <stdexcept>
#include <string>

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

void check_device(ID3D11Device *device, UINT wanted, bool tracked = false)
{
	require(device != nullptr, "creation returned no device");
	constexpr UINT mask = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
	if ((device->GetCreationFlags() & mask) != wanted)
		std::fprintf(stderr, "Device policy: wanted 0x%02x, received 0x%02x\n", wanted,
		             device->GetCreationFlags() & mask);
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
	if (tracked)
	{
		constexpr GUID tracker_guid{0x4a40bf2f, 0x6358, 0x470f, {0xba, 0x0a, 0x66, 0x2e, 0x3e, 0x2d, 0x8c, 0xd3}};
		ComPtr<IUnknown> tracker;
		UINT size = sizeof(IUnknown *);
		require(SUCCEEDED(buffer->GetPrivateData(tracker_guid, &size, tracker.GetAddressOf())) && tracker != nullptr,
		        "D3D11 resource tracking was lost after D3D12 helper creation");
	}
}

HRESULT check_d3d12_swap_chain(HWND window, const char *mode, bool invalid_device = false)
{
	auto create_factory =
	    exported<HRESULT(WINAPI *)(UINT, REFIID, void **)>(load_system(L"dxgi.dll"), "CreateDXGIFactory2");
	ComPtr<IDXGIFactory4> factory;
	require(SUCCEEDED(create_factory(0, IID_PPV_ARGS(&factory))), "D3D12 factory creation failed");
	ComPtr<IDXGIAdapter> warp;
	require(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))), "D3D12 WARP adapter unavailable");
	auto create_device = exported<decltype(&D3D12CreateDevice)>(load_system(L"d3d12.dll"), "D3D12CreateDevice");
	ComPtr<ID3D12Device> device;
	require(SUCCEEDED(create_device(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))),
	        "D3D12 WARP device creation failed");
	D3D12_COMMAND_QUEUE_DESC queue_desc{};
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ComPtr<ID3D12CommandQueue> queue;
	require(SUCCEEDED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue))),
	        "D3D12 direct queue creation failed");
	DXGI_SWAP_CHAIN_DESC1 description{};
	description.Width = 32;
	description.Height = 24;
	description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	description.SampleDesc.Count = 1;
	description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	description.BufferCount = 2;
	description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	IUnknown *creation_device = invalid_device ? nullptr : queue.Get();
	ComPtr<IDXGISwapChain1> swap;
	HRESULT creation_result;
	if (std::strcmp(mode, "d3d12-corewindow") == 0)
	{
		const HRESULT result =
		    factory->CreateSwapChainForCoreWindow(creation_device, nullptr, &description, nullptr, &swap);
		require(FAILED(result) && swap == nullptr, "invalid CoreWindow creation did not fail cleanly");
		return result;
	}
	if (std::strcmp(mode, "d3d12-legacy") == 0)
	{
		DXGI_SWAP_CHAIN_DESC legacy{};
		legacy.BufferDesc.Width = 32;
		legacy.BufferDesc.Height = 24;
		legacy.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		legacy.SampleDesc.Count = 1;
		legacy.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		legacy.BufferCount = 2;
		legacy.OutputWindow = window;
		legacy.Windowed = TRUE;
		legacy.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		ComPtr<IDXGISwapChain> base;
		creation_result = factory->CreateSwapChain(creation_device, &legacy, &base);
		if (invalid_device)
		{
			require(FAILED(creation_result) && base == nullptr, "null legacy swap-chain device was accepted");
			return creation_result;
		}
		require(SUCCEEDED(creation_result), "D3D12 legacy swap-chain creation failed");
		require(SUCCEEDED(base.As(&swap)), "D3D12 legacy swap chain lost IDXGISwapChain1");
	}
	else if (std::strcmp(mode, "d3d12-composition") == 0)
	{
		description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		description.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		creation_result = factory->CreateSwapChainForComposition(creation_device, &description, nullptr, &swap);
	}
	else
		creation_result =
		    factory->CreateSwapChainForHwnd(creation_device, window, &description, nullptr, nullptr, &swap);
	if (invalid_device)
	{
		require(FAILED(creation_result) && swap == nullptr, "null swap-chain device was accepted");
		return creation_result;
	}
	require(SUCCEEDED(creation_result), "D3D12 helper swap-chain creation failed");
	ComPtr<IDXGISwapChain4> modern;
	const HRESULT result = swap.As(&modern);
	if (FAILED(result))
		std::fprintf(stderr, "D3D12 IDXGISwapChain4 query returned 0x%08lx\n", static_cast<unsigned long>(result));
	require(SUCCEEDED(result) && modern != nullptr, "D3D12 helper swap chain lost IDXGISwapChain4");
	DXGI_SWAP_CHAIN_DESC1 actual{};
	require(SUCCEEDED(modern->GetDesc1(&actual)), "D3D12 swap-chain descriptor unavailable");
	require(actual.Width == 32 && actual.Height == 24 && actual.BufferCount == 2,
	        "D3D11 configuration changed the D3D12 swap-chain dimensions or buffer count");
	std::printf("PASS %s retains IDXGISwapChain4 and native dimensions\n", mode);
	return S_OK;
}

void check_feature_levels(HMODULE proxy, PFN_D3D11_CREATE_DEVICE create, PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN swap,
                          HWND window, const char *mode, UINT wanted, UINT buffers, bool chained)
{
	const D3D_FEATURE_LEVEL request_11_1[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	D3D_FEATURE_LEVEL negotiated = static_cast<D3D_FEATURE_LEVEL>(0);
	HRESULT hr = create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, request_11_1, 2, D3D11_SDK_VERSION, &device,
	                    &negotiated, &context);
	require(SUCCEEDED(hr), "D3D11CreateDevice rejected 11.1 feature level request");
	if (negotiated != D3D_FEATURE_LEVEL_11_1 || device->GetFeatureLevel() != D3D_FEATURE_LEVEL_11_1)
	{
		std::fprintf(stderr, "Feature level: wanted 0x%x, out 0x%x, device 0x%x\n",
		             static_cast<unsigned>(D3D_FEATURE_LEVEL_11_1), static_cast<unsigned>(negotiated),
		             static_cast<unsigned>(device->GetFeatureLevel()));
	}
	require(negotiated == D3D_FEATURE_LEVEL_11_1 && device->GetFeatureLevel() == D3D_FEATURE_LEVEL_11_1,
	        "D3D11CreateDevice did not preserve feature level 11.1");
	check_device(device.Get(), wanted, false);
	require(context != nullptr, "creation returned no context");
	ComPtr<ID3D11Device1> device1;
	require(SUCCEEDED(device.As(&device1)) && device1 != nullptr, "ID3D11Device1 unavailable with default settings");
	ComPtr<ID3D11DeviceContext1> context1;
	require(SUCCEEDED(context.As(&context1)) && context1 != nullptr,
	        "ID3D11DeviceContext1 unavailable with default settings");
	context->ClearState();
	std::printf("PASS %s creation 1 (device)\n", mode);

	device.Reset();
	context.Reset();
	device1.Reset();
	context1.Reset();
	ComPtr<IDXGISwapChain> swap_chain;
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
	const D3D_FEATURE_LEVEL request_12_0[] = {D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
	negotiated = static_cast<D3D_FEATURE_LEVEL>(0);
	hr = swap(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, request_12_0, 3, D3D11_SDK_VERSION, &description, &swap_chain,
	          &device, &negotiated, &context);
	require(SUCCEEDED(hr), "D3D11CreateDeviceAndSwapChain rejected 12.0/11.1 feature level request");
	require(swap_chain != nullptr, "creation returned no swap chain");
	if (negotiated < D3D_FEATURE_LEVEL_11_1 || device->GetFeatureLevel() != negotiated)
	{
		std::fprintf(stderr, "SwapChain feature level: out 0x%x, device 0x%x\n", static_cast<unsigned>(negotiated),
		             static_cast<unsigned>(device->GetFeatureLevel()));
	}
	require(negotiated >= D3D_FEATURE_LEVEL_11_1 && device->GetFeatureLevel() == negotiated,
	        "D3D11CreateDeviceAndSwapChain did not preserve >= 11.1 feature level");
	check_device(device.Get(), wanted, false);
	DXGI_SWAP_CHAIN_DESC actual = {};
	require(SUCCEEDED(swap_chain->GetDesc(&actual)), "swap-chain description unavailable");
	require(actual.BufferCount == buffers, "proxy swap-chain policy was bypassed");
	ComPtr<IDXGISwapChain4> swap_chain4;
	require(SUCCEEDED(swap_chain.As(&swap_chain4)) && swap_chain4 != nullptr,
	        "IDXGISwapChain4 unavailable on wrapped D3D11 swap chain");
	require(context != nullptr, "creation returned no context");
	context->ClearState();
	std::printf("PASS %s creation 2 (device+swap)\n", mode);

	auto create_factory2 =
	    exported<HRESULT(WINAPI *)(UINT, REFIID, void **)>(load_system(L"dxgi.dll"), "CreateDXGIFactory2");
	ComPtr<IDXGIFactory2> factory2;
	require(SUCCEEDED(create_factory2(0, IID_PPV_ARGS(&factory2))) && factory2 != nullptr,
	        "IDXGIFactory2 unavailable with default settings");
	ComPtr<IDXGIFactory4> factory4;
	require(SUCCEEDED(create_factory2(0, IID_PPV_ARGS(&factory4))) && factory4 != nullptr,
	        "IDXGIFactory4 unavailable via hooked CreateDXGIFactory2");
	ComPtr<IDXGIAdapter> warp_adapter;
	require(SUCCEEDED(factory4->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter))) && warp_adapter != nullptr,
	        "IDXGIFactory4 interface was corrupted by CreateDXGIFactory2 hook");
	if (chained)
	{
		wchar_t sys_d3d11[MAX_PATH] = {};
		require(GetSystemDirectoryW(sys_d3d11, MAX_PATH) != 0, "system directory unavailable");
		require(wcscat_s(sys_d3d11, L"\\d3d11.dll") == 0, "system d3d11 path overflow");
		HMODULE sys_module = LoadLibraryExW(sys_d3d11, nullptr, 0);
		require(sys_module != nullptr && sys_module != proxy,
		        "chained module redirected System32\\d3d11.dll back to outer proxy");
		FreeLibrary(sys_module);
	}
	std::printf("PASS %s modern interfaces\n", mode);

	device.Reset();
	context.Reset();
	swap_chain.Reset();
	swap_chain4.Reset();
	negotiated = static_cast<D3D_FEATURE_LEVEL>(0);
	if (std::strcmp(mode, "feature-level-force") == 0)
	{
		const D3D_FEATURE_LEVEL request_10_0[] = {D3D_FEATURE_LEVEL_10_0};
		hr = create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, request_10_0, 1, D3D11_SDK_VERSION, &device, &negotiated,
		            &context);
		require(SUCCEEDED(hr) && device != nullptr, "allow_create_device=2 failed to upgrade down-level request");
		require(negotiated == D3D_FEATURE_LEVEL_11_0 && device->GetFeatureLevel() == D3D_FEATURE_LEVEL_11_0,
		        "allow_create_device=2 did not upgrade down-level request to 11.0");
	}
	else
	{
		const D3D_FEATURE_LEVEL request_9_2[] = {D3D_FEATURE_LEVEL_9_2};
		hr = create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, request_9_2, 1, D3D11_SDK_VERSION, &device, &negotiated,
		            &context);
		require(hr == E_INVALIDARG && device == nullptr, "down-level feature level request was not rejected");
	}
	std::printf("PASS %s downlevel policy\n", mode);
}

void run(const char *mode)
{
	load_system(L"dxgi.dll");
	const bool d3d12 = std::strncmp(mode, "d3d12-", 6) == 0;
	const HRESULT native_null_device = d3d12 ? check_d3d12_swap_chain(nullptr, mode, true) : S_OK;
	const HRESULT native_failure =
	    std::strcmp(mode, "d3d12-corewindow") == 0 ? check_d3d12_swap_chain(nullptr, mode) : S_OK;
	wchar_t path[MAX_PATH] = {};
	require(GetModuleFileNameW(nullptr, path, MAX_PATH) != 0, "executable path unavailable");
	wchar_t *leaf = wcsrchr(path, L'\\');
	require(leaf != nullptr, "executable directory unavailable");
	*(leaf + 1) = L'\0';
	HMODULE retainer = nullptr;
	const bool shutdown = std::strstr(mode, "shutdown") != nullptr;
	if (shutdown)
	{
		wchar_t retain_path[MAX_PATH] = {};
		require(wcscpy_s(retain_path, path) == 0 && wcscat_s(retain_path, L"ResourceRetainer.dll") == 0,
		        "retainer path overflow");
		retainer = LoadLibraryW(retain_path);
		require(retainer != nullptr, "resource retainer unavailable");
	}
	require(wcscat_s(path, L"d3d11.dll") == 0, "proxy path overflow");
	HMODULE proxy = LoadLibraryW(path);
	require(proxy != nullptr, "could not load game-local proxy");
	const bool chain = std::strncmp(mode, "chain-", 6) == 0;
	const bool downstream = std::strstr(mode, "downstream") != nullptr;
	if (std::strstr(mode, "early") != nullptr)
	{
		auto initialize = exported<BOOL(WINAPI *)()>(proxy, "ChainInitialize");
		require(initialize() != FALSE, "early chain initialization failed");
	}
	auto create = exported<PFN_D3D11_CREATE_DEVICE>(proxy, "D3D11CreateDevice");
	auto swap = exported<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(proxy, "D3D11CreateDeviceAndSwapChain");
	const bool edup = std::strncmp(mode, "edup", 4) == 0;
	if (edup)
	{
		// A successful capability-only probe does not give EDUP a device to
		// attach to. The later real creation must still reach its exports.
		D3D_FEATURE_LEVEL level = {};
		require(SUCCEEDED(create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, nullptr,
		                         &level, nullptr)),
		        "capability-only probe failed");
	}
	// The composed contract is BGRA (0x20) plus single-threaded (0x01).
	const UINT wanted = chain && downstream ? 0x21 : chain ? 0x20 : downstream ? 0x01 : 0;
	const UINT buffers = downstream ? 3 : chain ? 2 : 1;
	const bool swap_first = edup || std::strstr(mode, "swap-early") != nullptr;
	WNDCLASSW window_class = {};
	window_class.lpfnWndProc = DefWindowProcW;
	window_class.hInstance = GetModuleHandleW(nullptr);
	window_class.lpszClassName = L"EdhmProxyChainFixture";
	require(RegisterClassW(&window_class) != 0, "window registration failed");
	HWND window = CreateWindowW(window_class.lpszClassName, L"EDHM off-game proxy check", WS_OVERLAPPEDWINDOW, 0, 0, 64,
	                            64, nullptr, nullptr, window_class.hInstance, nullptr);
	require(window != nullptr, "hidden test window unavailable");
	// EDUP deliberately owns one rendering device per process. Its real-DLL
	// check uses that one device; the boundary fixture covers repeated creation.
	const bool feature_level = std::strstr(mode, "feature-level") != nullptr;
	if (feature_level)
	{
		check_feature_levels(proxy, create, swap, window, mode, wanted, buffers, chain);
		DestroyWindow(window);
		return;
	}
	const unsigned passes = edup || shutdown ? 1 : 4;
	for (unsigned pass = 0; pass < passes; ++pass)
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
		}
		else
		{
			hr = create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr,
			            &context);
			require(SUCCEEDED(hr), "device creation failed");
		}
		check_device(device.Get(), wanted, d3d12);
		if (d3d12 && pass == 0)
		{
			require(check_d3d12_swap_chain(nullptr, mode, true) == native_null_device,
			        "null swap-chain device changed the native HRESULT");
			require(check_d3d12_swap_chain(window, mode) == native_failure,
			        "D3D12 helper creation changed the native HRESULT");
		}
		if (shutdown)
		{
			auto retain = exported<HRESULT(WINAPI *)(ID3D11Device *)>(retainer, "RetainResources");
			require(SUCCEEDED(retain(device.Get())), "retaining tracked buffer/view failed");
		}

		if (swap_chain)
		{
			DXGI_SWAP_CHAIN_DESC actual = {};
			require(SUCCEEDED(swap_chain->GetDesc(&actual)), "swap-chain description unavailable");
			require(actual.BufferCount == buffers, "proxy swap-chain policy was bypassed");
		}
		require(context != nullptr, "creation returned no context");
		context->ClearState();
		if (edup && swap_chain)
			require(SUCCEEDED(swap_chain->Present(0, 0)), "EDUP presentation failed");
		std::printf("PASS %s creation %u (%s)\n", mode, pass + 1, with_swap ? "device+swap" : "device");
	}
	DestroyWindow(window);
	if (std::strcmp(mode, "standalone-unload") == 0)
	{
		require(FreeLibrary(proxy) != FALSE, "ordinary DLL unload failed");
		require(GetModuleHandleW(path) == nullptr, "product DLL remained loaded after ordinary unload");
		std::printf("PASS ordinary unload after object release\n");
	}
}
} // namespace

int main(int argc, char **argv)
{
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
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
