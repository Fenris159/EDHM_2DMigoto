// A third-party D3D11 proxy boundary. Its public effect is BGRA device support
// and a two-buffer swap chain; the downstream fixture enables single threading.
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstring>
#include <cwchar>

namespace
{
HMODULE self_module;
INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;
PFN_D3D11_CREATE_DEVICE system_create;
PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN system_swap;
PFN_D3D11_CREATE_DEVICE inner_create;
PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN inner_swap;
thread_local unsigned create_depth;
bool downstream;
// Retain the actual private-data callback beyond product CRT teardown. Release
// GPU objects while WARP's workers are alive, before Windows terminates them.
struct RetainedResources
{
	Microsoft::WRL::ComPtr<IUnknown> tracker;
	HANDLE completion = INVALID_HANDLE_VALUE;
	RetainedResources() = default;
	RetainedResources(const RetainedResources &) = delete;
	RetainedResources &operator=(const RetainedResources &) = delete;
	RetainedResources(RetainedResources &&) = delete;
	RetainedResources &operator=(RetainedResources &&) = delete;
	~RetainedResources()
	{
		if (tracker && tracker.Detach()->Release() == 0)
		{
			constexpr char message[] = "released\n";
			DWORD written;
			WriteFile(completion, message, sizeof(message) - 1, &written, nullptr);
		}
		if (completion != INVALID_HANDLE_VALUE)
			CloseHandle(completion);
	}
} retained_resources;

template <typename T> T exported(HMODULE module, const char *name)
{
	FARPROC address = GetProcAddress(module, name);
	T result = nullptr;
	static_assert(sizeof(result) == sizeof(address));
	std::memcpy(&result, &address, sizeof(result));
	return result;
}

BOOL CALLBACK initialize(PINIT_ONCE, PVOID, PVOID *)
{
	wchar_t path[MAX_PATH] = {};
	if (!GetSystemDirectoryW(path, MAX_PATH) || wcscat_s(path, L"\\d3d11.dll"))
		return FALSE;
	HMODULE backend = LoadLibraryW(path);
	if (!backend)
		return FALSE;
	system_create = exported<PFN_D3D11_CREATE_DEVICE>(backend, "D3D11CreateDevice");
	system_swap = exported<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(backend, "D3D11CreateDeviceAndSwapChain");
	if (!system_create || !system_swap)
		return FALSE;
	if (!GetModuleFileNameW(self_module, path, MAX_PATH))
		return FALSE;
	wchar_t *leaf = wcsrchr(path, L'\\');
	if (!leaf)
		return FALSE;
	downstream = _wcsicmp(leaf + 1, L"downstream.dll") == 0;
	if (!downstream)
	{
		*(leaf + 1) = L'\0';
		if (wcscat_s(path, L"d3d11_edhm.dll"))
			return FALSE;
		HMODULE inner = LoadLibraryW(path);
		if (!inner)
			return FALSE;
		inner_create = exported<PFN_D3D11_CREATE_DEVICE>(inner, "D3D11CreateDevice");
		inner_swap = exported<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(inner, "D3D11CreateDeviceAndSwapChain");
		if (!inner_create || !inner_swap)
			return FALSE;
	}
	return TRUE;
}

struct Creation
{
	Creation()
	{
		++create_depth;
	}
	~Creation()
	{
		--create_depth;
	}
};
} // namespace

extern "C" HRESULT WINAPI RetainResources(ID3D11Device *device)
{
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = 16;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	HRESULT result = device->CreateBuffer(&desc, nullptr, &buffer);
	if (FAILED(result))
		return result;
	D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
	view_desc.Format = DXGI_FORMAT_R32_FLOAT;
	view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	view_desc.Buffer.NumElements = 4;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
	result = device->CreateShaderResourceView(buffer.Get(), &view_desc, &view);
	if (FAILED(result))
		return result;
	// GetPrivateData returns an owned interface reference, just as a D3D
	// resource keeps one internally. The final callback is the code under test.
	constexpr GUID tracker_guid{0x4a40bf2f, 0x6358, 0x470f, {0xba, 0x0a, 0x66, 0x2e, 0x3e, 0x2d, 0x8c, 0xd3}};
	UINT size = sizeof(IUnknown *);
	result = buffer->GetPrivateData(tracker_guid, &size, retained_resources.tracker.GetAddressOf());
	if (FAILED(result))
		return result;
	if (size != sizeof(IUnknown *) || !retained_resources.tracker)
		return E_UNEXPECTED;
	retained_resources.completion = CreateFileW(L"shutdown-complete.txt", GENERIC_WRITE, FILE_SHARE_READ, nullptr,
	                                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (retained_resources.completion == INVALID_HANDLE_VALUE)
		return HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

extern "C" BOOL WINAPI ChainInitialize()
{
	return InitOnceExecuteOnce(&init_once, initialize, nullptr, nullptr);
}

extern "C" HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter *adapter, D3D_DRIVER_TYPE type, HMODULE software, UINT flags,
                                            const D3D_FEATURE_LEVEL *levels, UINT count, UINT sdk,
                                            ID3D11Device **device, D3D_FEATURE_LEVEL *level,
                                            ID3D11DeviceContext **context)
{
	if (!ChainInitialize())
		return E_FAIL;
	Creation scope;
	auto target = !downstream && create_depth == 1 ? inner_create : system_create;
	flags |= downstream ? D3D11_CREATE_DEVICE_SINGLETHREADED : D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	return target(adapter, type, software, flags, levels, count, sdk, device, level, context);
}

extern "C" HRESULT WINAPI D3D11CreateDeviceAndSwapChain(IDXGIAdapter *adapter, D3D_DRIVER_TYPE type, HMODULE software,
                                                        UINT flags, const D3D_FEATURE_LEVEL *levels, UINT count,
                                                        UINT sdk, const DXGI_SWAP_CHAIN_DESC *description,
                                                        IDXGISwapChain **swap, ID3D11Device **device,
                                                        D3D_FEATURE_LEVEL *level, ID3D11DeviceContext **context)
{
	if (!ChainInitialize())
		return E_FAIL;
	Creation scope;
	auto target = !downstream && create_depth == 1 ? inner_swap : system_swap;
	DXGI_SWAP_CHAIN_DESC changed = {};
	if (description)
	{
		changed = *description;
		changed.BufferCount = downstream ? 3 : 2;
		description = &changed;
	}
	flags |= downstream ? D3D11_CREATE_DEVICE_SINGLETHREADED : D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	return target(adapter, type, software, flags, levels, count, sdk, description, swap, device, level, context);
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
		self_module = module;
	return TRUE;
}
