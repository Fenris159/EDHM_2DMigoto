// The actual production D3D upload adapter; no game, swap chain or Present.
#include "../DirectX11/IniParamsD3DUpload.h"
#include "../DirectX11/IniParamsPublication.h"
#include "D3DMapBoundary.h"

#include <wrl/client.h>

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
using Microsoft::WRL::ComPtr;
using Image = std::array<float, 8>;

void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

ComPtr<ID3D11Texture1D> texture(ID3D11Device *device, const Image &image)
{
	D3D11_TEXTURE1D_DESC desc{};
	desc.Width = 2;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = image.data();
	ComPtr<ID3D11Texture1D> result;
	require(SUCCEEDED(device->CreateTexture1D(&desc, &initial, &result)), "texture creation failed");
	return result;
}

Image readback(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11Texture1D *source)
{
	D3D11_TEXTURE1D_DESC desc{};
	source->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ComPtr<ID3D11Texture1D> staging;
	require(SUCCEEDED(device->CreateTexture1D(&desc, nullptr, &staging)), "staging creation failed");
	context->CopyResource(staging.Get(), source);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	require(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "GPU readback failed");
	Image result{};
	std::memcpy(result.data(), mapped.pData, sizeof(result));
	context->Unmap(staging.Get(), 0);
	return result;
}

void exercise(ID3D11Device *device, ID3D11DeviceContext *context)
{
	Image cpu{5, 6, 7, 8, 50, 60, 70, 80};
	auto target = texture(device, Image{1, 2, 3, 4, 10, 20, 30, 40});
	IniParamsPublication publication;
	auto upload = [&]() { return SUCCEEDED(UploadIniParams(context, target.Get(), cpu.data(), 2)); };
	{
		D3DMapBoundary boundary(context, target.Get());
		boundary.failures = 1;
		require(!publication.Publish(true, upload), "failed Map acknowledged publication");
		require(readback(device, context, target.Get()) == Image{1, 2, 3, 4, 10, 20, 30, 40},
		        "failed Map changed GPU contents");
	}
	cpu = Image{9, 10, 11, 12, 90, 100, 110, 120};
	require(publication.Publish(false, upload), "pending publication was lost after failed Map");
	require(readback(device, context, target.Get()) == Image{9, 10, 11, 12, 90, 100, 110, 120},
	        "retry did not publish the newest complete image");
	const Image first = readback(device, context, target.Get());
	cpu = Image{4, 3, 2, 1, 40, 30, 20, 10};
	require(publication.Publish(true, upload), "next publication failed");
	require(first == Image{9, 10, 11, 12, 90, 100, 110, 120}, "first publication lost its image");
	require(readback(device, context, target.Get()) == Image{4, 3, 2, 1, 40, 30, 20, 10},
	        "second publication lost its image");

	target.Reset();
	require(!publication.Publish(true, upload), "missing texture acknowledged publication");
	target = texture(device, Image{0, 0, 0, 0, 0, 0, 0, 0});
	require(publication.Publish(false, upload), "recreated texture lost pending publication");
	require(readback(device, context, target.Get()) == Image{4, 3, 2, 1, 40, 30, 20, 10},
	        "recreated texture did not receive complete current contents");
}
} // namespace

int main(int argc, char **argv)
{
	try
	{
		const bool allow_hardware = argc == 2 && std::string(argv[1]) == "--allow-hardware";
		require(argc == 1 || allow_hardware, "usage: IniParamsUploadTests [--allow-hardware]");
		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> context;
		HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
		                                   &device, nullptr, &context);
		if (FAILED(result) && allow_hardware)
		{
			std::cout << "LIMITATION WARP unavailable; using explicitly allowed D3D hardware path\n";
			result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
			                           &device, nullptr, &context);
		}
		require(SUCCEEDED(result), "D3D11 device creation failed");
		exercise(device.Get(), context.Get());
		std::cout
		    << "PASS actual D3D upload: failure/retry, complete images, separate publication points, recreation\n";
		return 0;
	}
	catch (const std::exception &error)
	{
		std::cerr << "FAIL " << error.what() << '\n';
		return 1;
	}
}
