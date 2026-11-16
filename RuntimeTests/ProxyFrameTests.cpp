// Read actual product commands back from the GPU while an external Present
// replacement keeps the game's swap chain from ever reaching native Present.
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include "ImportBoundary.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace
{
using Microsoft::WRL::ComPtr;
ULONGLONG tick;
int held_key;

ULONGLONG WINAPI frame_tick()
{
	return tick;
}

SHORT WINAPI key_state(int key)
{
	return key == held_key ? static_cast<SHORT>(0x8000) : 0;
}

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
	require(result != nullptr, name);
	return result;
}

void values(ID3D11Device *device, ID3D11DeviceContext *context, float x, float y, float z = 0, float w = 0)
{
	ComPtr<ID3D11ShaderResourceView> view;
	context->PSGetShaderResources(120, 1, &view);
	require(view != nullptr, "IniParams not bound");
	ComPtr<ID3D11Resource> resource;
	view->GetResource(&resource);
	ComPtr<ID3D11Texture1D> texture;
	require(SUCCEEDED(resource.As(&texture)), "IniParams not a texture");
	D3D11_TEXTURE1D_DESC desc{};
	texture->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.MiscFlags = 0;
	ComPtr<ID3D11Texture1D> staging;
	require(SUCCEEDED(device->CreateTexture1D(&desc, nullptr, &staging)), "staging failed");
	context->CopyResource(staging.Get(), texture.Get());
	D3D11_MAPPED_SUBRESOURCE mapped{};
	require(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "readback failed");
	float actual[4]{};
	std::memcpy(actual, mapped.pData, sizeof(actual));
	context->Unmap(staging.Get(), 0);
	std::printf("commands x=%.0f y=%.0f expected %.0f %.0f\n", actual[0], actual[1], x, y);
	require(actual[0] == x, "Present commands were starved or duplicated");
	require(actual[1] == y, "post-Present commands were starved or duplicated");
	std::printf("inputs z=%.0f w=%.0f expected %.0f %.0f\n", actual[2], actual[3], z, w);
	require(actual[2] == z, "hotkey or reload value did not apply");
	require(actual[3] == w, "preset transition or reload value did not apply");
}

HRESULT present(IDXGISwapChain *chain, UINT flags, bool modern)
{
	if (!modern)
		return chain->Present(0, flags);
	ComPtr<IDXGISwapChain1> one;
	HRESULT hr = chain->QueryInterface(IID_PPV_ARGS(&one));
	if (FAILED(hr))
		return hr;
	DXGI_PRESENT_PARAMETERS parameters{};
	return one->Present1(0, flags, &parameters);
}

struct WorkerPresent
{
	IDXGISwapChain *chain;
	bool modern;
	HRESULT result = E_UNEXPECTED;
	static DWORD WINAPI run(void *argument)
	{
		auto &worker = *static_cast<WorkerPresent *>(argument);
		worker.result = present(worker.chain, 0, worker.modern);
		return 0;
	}
};

void run(bool hardware, bool controls, bool modern)
{
	HMODULE product = LoadLibraryW(L".\\d3d11.dll");
	require(product != nullptr, "product DLL unavailable");
	auto create = exported<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(product, "D3D11CreateDeviceAndSwapChain");
	HMODULE proxy = LoadLibraryW(L".\\FrameProxyFixture.dll");
	require(proxy != nullptr, "proxy fixture unavailable");
	auto install = exported<BOOL(WINAPI *)(IDXGISwapChain *)>(proxy, "InstallFrameProxy");
	auto mode = exported<void(WINAPI *)(unsigned)>(proxy, "SetFrameProxyMode");
	auto read = exported<HRESULT(WINAPI *)(IDXGISwapChain *)>(proxy, "ReadFrameProxyBuffer");
	auto remove = exported<BOOL(WINAPI *)(IDXGISwapChain *)>(proxy, "RemoveFrameProxy");
	WNDCLASSW wc{};
	wc.lpfnWndProc = DefWindowProcW;
	wc.hInstance = GetModuleHandleW(nullptr);
	wc.lpszClassName = L"EdhmProxyFrameFixture";
	require(RegisterClassW(&wc) != 0, "window registration failed");
	HWND window = CreateWindowW(wc.lpszClassName, L"EDHM frame probe", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr,
	                            nullptr, wc.hInstance, nullptr);
	require(window != nullptr, "window creation failed");
	DXGI_SWAP_CHAIN_DESC desc{};
	desc.BufferDesc.Width = 64;
	desc.BufferDesc.Height = 64;
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 1;
	desc.OutputWindow = window;
	desc.Windowed = TRUE;
	ComPtr<IDXGISwapChain> chain;
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	require(SUCCEEDED(create(nullptr, hardware ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr,
	                         0, D3D11_SDK_VERSION, &desc, &chain, &device, nullptr, &context)),
	        "WARP creation failed");
	require(install(chain.Get()) != FALSE, "proxy install failed");
	ComPtr<ID3D11Texture2D> back;
	require(SUCCEEDED(chain->GetBuffer(0, IID_PPV_ARGS(&back))), "back buffer unavailable");
	tick = GetTickCount64() + 1;
	ImportBoundary clock(product, "GetTickCount64", &frame_tick);
	ImportBoundary keys(product, "GetAsyncKeyState", &key_state);
	// A zero-draw/menu frame is still a game frame. No timing or draw threshold.
	require(SUCCEEDED(present(chain.Get(), 0, modern)), "proxy Present failed");
	values(device.Get(), context.Get(), 1, 1);
	if (controls)
	{
		held_key = VK_F6;
		tick += 500;
		require(SUCCEEDED(present(chain.Get(), 0, modern)), "hotkey Present failed");
		values(device.Get(), context.Get(), 2, 2, 9, 4);
		// Holding the toggle must not activate it twice. The preset reaches its
		// hand-computed endpoint after the second controlled half-second.
		tick += 500;
		require(SUCCEEDED(present(chain.Get(), 0, modern)), "transition Present failed");
		values(device.Get(), context.Get(), 3, 3, 9, 8);
		require(CopyFileW(L"reload.ini", L"d3dx.ini", FALSE) != FALSE, "reload input copy failed");
		held_key = VK_F11;
		require(SUCCEEDED(present(chain.Get(), 0, modern)), "reload hotkey Present failed");
		values(device.Get(), context.Get(), 10, 21, 30, 40);
		DestroyWindow(window);
		return;
	}
	require(SUCCEEDED(read(chain.Get())) && SUCCEEDED(read(chain.Get())), "proxy buffer reads failed");
	require(SUCCEEDED(present(chain.Get(), DXGI_PRESENT_TEST, modern)), "test Present failed");
	values(device.Get(), context.Get(), 1, 1);
	mode(2);
	require(present(chain.Get(), 0, modern) == E_FAIL, "proxy failure was not preserved");
	values(device.Get(), context.Get(), 2, 2);
	mode(1);
	require(SUCCEEDED(present(chain.Get(), 0, modern)), "forwarding proxy Present failed");
	values(device.Get(), context.Get(), 3, 3);
	mode(0);
	WorkerPresent worker{chain.Get(), modern};
	HANDLE thread = CreateThread(nullptr, 0, WorkerPresent::run, &worker, 0, nullptr);
	require(thread != nullptr, "worker creation failed");
	DWORD wait = WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
	require(wait == WAIT_OBJECT_0 && SUCCEEDED(worker.result), "worker Present failed");
	values(device.Get(), context.Get(), 3, 3);
	require(SUCCEEDED(present(chain.Get(), 0, modern)), "render-thread Present did not resume");
	values(device.Get(), context.Get(), 4, 4);
	require(remove(chain.Get()) != FALSE, "proxy removal failed");
	require(SUCCEEDED(present(chain.Get(), 0, modern)), "ordinary Present did not resume");
	values(device.Get(), context.Get(), 5, 5);
	require(install(chain.Get()) != FALSE, "proxy reinstall failed");
	require(SUCCEEDED(chain->GetBuffer(0, IID_PPV_ARGS(back.ReleaseAndGetAddressOf()))), "buffer reread failed");
	require(SUCCEEDED(present(chain.Get(), 0, modern)), "reinstalled proxy failed");
	values(device.Get(), context.Get(), 6, 6);
	// A second wrapped chain shares the patched vtable, but is not this game's
	// established frame source. It must not tick the global command lists.
	ComPtr<IDXGISwapChain> other;
	ComPtr<ID3D11Device> other_device;
	ComPtr<ID3D11DeviceContext> other_context;
	require(SUCCEEDED(create(nullptr, hardware ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr,
	                         0, D3D11_SDK_VERSION, &desc, &other, &other_device, nullptr, &other_context)),
	        "second chain creation failed");
	require(SUCCEEDED(present(other.Get(), 0, modern)), "other chain Present failed");
	values(device.Get(), context.Get(), 6, 6);
	values(other_device.Get(), other_context.Get(), 6, 6);
	other.Reset();
	other_context.Reset();
	other_device.Reset();
	back.Reset();
	require(chain.Detach()->Release() == 0, "primary chain still retained");
	require(SUCCEEDED(create(nullptr, hardware ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr,
	                         0, D3D11_SDK_VERSION, &desc, &other, &other_device, nullptr, &other_context)),
	        "replacement chain creation failed");
	require(SUCCEEDED(other->GetBuffer(0, IID_PPV_ARGS(&back))), "replacement buffer unavailable");
	require(SUCCEEDED(present(other.Get(), 0, modern)), "replacement chain Present failed");
	values(other_device.Get(), other_context.Get(), 7, 7);
	DestroyWindow(window);
}
} // namespace

int main(int argc, char **argv)
{
	try
	{
		require(argc == 2 || (argc == 3 && std::strcmp(argv[2], "hardware") == 0), "expected flow|controls [hardware]");
		bool modern = std::strcmp(argv[1], "flow1") == 0 || std::strcmp(argv[1], "controls1") == 0;
		bool controls = std::strcmp(argv[1], "controls") == 0 || std::strcmp(argv[1], "controls1") == 0;
		require(controls || modern || std::strcmp(argv[1], "flow") == 0, "unknown frame case");
		run(argc == 3, controls, modern);
		std::puts("PASS proxy frame actions");
		return 0;
	}
	catch (const std::exception &error)
	{
		std::fprintf(stderr, "FAIL: %s\n", error.what());
		return 1;
	}
}
