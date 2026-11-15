// Inspect a real product DLL's rendered reload notification and reloaded GPU data.
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

void check_reloaded_constants(ID3D11Device *device, ID3D11DeviceContext *context)
{
	ComPtr<ID3D11ShaderResourceView> view;
	context->PSGetShaderResources(120, 1, &view);
	require(view != nullptr, "reloaded IniParams are not bound");
	ComPtr<ID3D11Resource> resource;
	view->GetResource(&resource);
	ComPtr<ID3D11Texture1D> texture;
	require(SUCCEEDED(resource.As(&texture)), "IniParams are not a texture");
	D3D11_TEXTURE1D_DESC description = {};
	texture->GetDesc(&description);
	require(description.Width == 302, "reload did not apply the new IniParams extent");
	description.Usage = D3D11_USAGE_STAGING;
	description.BindFlags = 0;
	description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	description.MiscFlags = 0;
	ComPtr<ID3D11Texture1D> staging;
	require(SUCCEEDED(device->CreateTexture1D(&description, nullptr, &staging)), "IniParams staging failed");
	context->CopyResource(staging.Get(), texture.Get());
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	require(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "IniParams readback failed");
	float value = 0;
	std::memcpy(&value, mapped.pData, sizeof(value));
	context->Unmap(staging.Get(), 0);
	require(value == 7.0f, "reload did not apply the new constant value");
}

void check_image(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11Texture2D *back_buffer, bool show,
                 bool warnings)
{
	D3D11_TEXTURE2D_DESC description = {};
	back_buffer->GetDesc(&description);
	description.Usage = D3D11_USAGE_STAGING;
	description.BindFlags = 0;
	description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	description.MiscFlags = 0;
	ComPtr<ID3D11Texture2D> staging;
	require(SUCCEEDED(device->CreateTexture2D(&description, nullptr, &staging)), "image staging failed");
	context->CopyResource(staging.Get(), back_buffer);
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	require(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "image readback failed");
	unsigned green = 0, orange = 0, nonblack = 0;
	for (UINT row = 0; row < description.Height; ++row)
	{
		const auto *pixels = static_cast<const unsigned char *>(mapped.pData) + row * mapped.RowPitch;
		for (UINT column = 0; column < description.Width; ++column)
		{
			const unsigned red = pixels[column * 4], g = pixels[column * 4 + 1], blue = pixels[column * 4 + 2];
			green += g > red * 2 + 20 && g > blue * 2 + 20;
			orange += red > g && g > blue + 20;
			nonblack += red != 0 || g != 0 || blue != 0;
		}
	}
	context->Unmap(staging.Get(), 0);
	std::printf("image green=%u orange=%u nonblack=%u\n", green, orange, nonblack);
	// Glyph counts vary with the duration text; their colors identify the notices.
	if (show)
		require(green != 0, "default reload notification was not visible");
	else
		require(green == 0, "suppressed reload notification remained visible");
	if (warnings)
		require(orange != 0, "suppressing the reload notification also hid warnings");
	else
		require(orange == 0, "disabled warnings remained visible");
	if (!show && !warnings)
		require(nonblack == 0, "quiet reload changed the cleared image");
}

void run(const char *mode, bool hardware)
{
	const bool show = std::strcmp(mode, "default") == 0;
	const bool warnings = std::strcmp(mode, "warnings") == 0;
	require(show || warnings || std::strcmp(mode, "quiet") == 0, "unknown notification case");
	wchar_t path[MAX_PATH] = {};
	require(GetSystemDirectoryW(path, MAX_PATH) != 0 && wcscat_s(path, L"\\dxgi.dll") == 0,
	        "system DXGI path unavailable");
	require(LoadLibraryW(path) != nullptr, "system DXGI unavailable");
	require(GetModuleFileNameW(nullptr, path, MAX_PATH) != 0, "executable path unavailable");
	wchar_t *leaf = wcsrchr(path, L'\\');
	require(leaf != nullptr, "executable directory unavailable");
	*(leaf + 1) = L'\0';
	require(wcscat_s(path, L"d3d11.dll") == 0, "product path overflow");
	HMODULE module = LoadLibraryW(path);
	require(module != nullptr, "product DLL did not load");
	FARPROC address = GetProcAddress(module, "D3D11CreateDeviceAndSwapChain");
	PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN create = nullptr;
	static_assert(sizeof(create) == sizeof(address));
	std::memcpy(&create, &address, sizeof(create));
	require(create != nullptr, "creation export missing");
	WNDCLASSW window_class = {};
	window_class.lpfnWndProc = DefWindowProcW;
	window_class.hInstance = GetModuleHandleW(nullptr);
	window_class.lpszClassName = L"EdhmReloadNoticeFixture";
	require(RegisterClassW(&window_class) != 0, "window registration failed");
	HWND window = CreateWindowW(window_class.lpszClassName, L"EDHM notification check", WS_OVERLAPPEDWINDOW, 0, 0, 640,
	                            160, nullptr, nullptr, window_class.hInstance, nullptr);
	require(window != nullptr, "hidden test window unavailable");
	DXGI_SWAP_CHAIN_DESC description = {};
	description.BufferDesc.Width = 640;
	description.BufferDesc.Height = 160;
	description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	description.SampleDesc.Count = 1;
	description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	description.BufferCount = 1;
	description.OutputWindow = window;
	description.Windowed = TRUE;
	// Retain the rendered image for inspection after the one correctness Present.
	description.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
	ComPtr<IDXGISwapChain> swap_chain;
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	require(SUCCEEDED(create(nullptr, hardware ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr,
	                         0, D3D11_SDK_VERSION, &description, &swap_chain, &device, nullptr, &context)),
	        "device and swap-chain creation failed");
	ComPtr<ID3D11Texture2D> back_buffer;
	require(SUCCEEDED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer))), "back buffer unavailable");
	ComPtr<ID3D11RenderTargetView> target;
	require(SUCCEEDED(device->CreateRenderTargetView(back_buffer.Get(), nullptr, &target)), "render target failed");
	const float black[] = {0, 0, 0, 1};
	context->ClearRenderTargetView(target.Get(), black);
	require(CopyFileW(L"reload.ini", L"d3dx.ini", FALSE) != FALSE, "reload input copy failed");
	require(SUCCEEDED(swap_chain->Present(0, 0)), "notification Present failed");
	check_reloaded_constants(device.Get(), context.Get());
	check_image(device.Get(), context.Get(), back_buffer.Get(), show, warnings);
	DestroyWindow(window);
}
} // namespace

int main(int argc, char **argv)
{
	try
	{
		require(argc == 2 || (argc == 3 && std::strcmp(argv[2], "hardware") == 0), "expected mode [hardware]");
		run(argv[1], argc == 3);
		std::printf("PASS reload notification %s\n", argv[1]);
		return 0;
	}
	catch (const std::exception &error)
	{
		std::fprintf(stderr, "FAIL: %s\n", error.what());
		return 1;
	}
}
