// Read actual product commands back from the GPU while an external Present
// replacement keeps the game's swap chain from ever reaching native Present.
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstring>
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
	require(result != nullptr, name);
	return result;
}

void values(ID3D11Device *device, ID3D11DeviceContext *context, float x, float y)
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
	float actual[2]{};
	std::memcpy(actual, mapped.pData, sizeof(actual));
	context->Unmap(staging.Get(), 0);
	std::printf("commands x=%.0f y=%.0f expected %.0f %.0f\n", actual[0], actual[1], x, y);
	require(actual[0] == x, "Present commands were starved or duplicated");
	require(actual[1] == y, "post-Present commands were starved or duplicated");
}

void run(bool hardware)
{
	HMODULE product = LoadLibraryW(L".\\d3d11.dll");
	require(product != nullptr, "product DLL unavailable");
	auto create = exported<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(product, "D3D11CreateDeviceAndSwapChain");
	HMODULE proxy = LoadLibraryW(L".\\FrameProxyFixture.dll");
	require(proxy != nullptr, "proxy fixture unavailable");
	auto install = exported<BOOL(WINAPI *)(IDXGISwapChain *)>(proxy, "InstallFrameProxy");
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
	// A zero-draw/menu frame is still a game frame. No timing or draw threshold.
	require(SUCCEEDED(chain->Present(0, 0)), "proxy Present failed");
	values(device.Get(), context.Get(), 1, 1);
	DestroyWindow(window);
}
} // namespace

int main(int argc, char **argv)
{
	try
	{
		require(argc == 1 || (argc == 2 && std::strcmp(argv[1], "hardware") == 0), "expected [hardware]");
		run(argc == 2);
		std::puts("PASS proxy frame actions");
		return 0;
	}
	catch (const std::exception &error)
	{
		std::fprintf(stderr, "FAIL: %s\n", error.what());
		return 1;
	}
}
