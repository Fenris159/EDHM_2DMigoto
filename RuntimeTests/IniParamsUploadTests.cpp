// The actual production D3D upload adapter; no game, swap chain or Present.
#include "../DirectX11/IniParamsD3DUpload.h"
#include "../DirectX11/IniParamsPublication.h"
#include "D3DMapBoundary.h"

#include <wrl/client.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>
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

// Compare only successful upload kernels, with the same compiler, context and
// texture. The existing kernel is the Map/copy/Unmap body from v0.1.8's checked
// command flush. This is not a shipped-DLL or original-3Dmigoto comparison.
void benchmark_uploads(ID3D11Device *device, ID3D11DeviceContext *context, int samples, int iterations)
{
	std::cout << "BENCH,row_count,sample,order,variant,iterations,elapsed_ns\n";
	for (UINT rows : {1u, 341u})
	{
		std::vector<float> image(static_cast<std::size_t>(rows) * 4);
		for (UINT i = 0; i < rows; ++i)
		{
			image[i * 4] = 1;
			image[i * 4 + 1] = 2;
			image[i * 4 + 2] = 3;
			image[i * 4 + 3] = 4;
		}
		D3D11_TEXTURE1D_DESC desc{};
		desc.Width = rows;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		D3D11_SUBRESOURCE_DATA initial{};
		initial.pSysMem = image.data();
		ComPtr<ID3D11Texture1D> target;
		require(SUCCEEDED(device->CreateTexture1D(&desc, &initial, &target)), "benchmark texture creation failed");
		auto legacy = [&]()
		{
			D3D11_MAPPED_SUBRESOURCE mapped{};
			require(SUCCEEDED(context->Map(target.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)),
			        "benchmark Map failed");
			std::memcpy(mapped.pData, image.data(), image.size() * sizeof(float));
			context->Unmap(target.Get(), 0);
		};
		auto candidate = [&]()
		{
			require(SUCCEEDED(UploadIniParams(context, target.Get(), image.data(), rows)),
			        "benchmark publication failed");
		};
		for (int warm = 0; warm < 256; ++warm)
		{
			legacy();
			candidate();
		}
		for (int sample = 0; sample < samples; ++sample)
		{
			for (int position = 0; position < 4; ++position)
			{
				const bool checked = ((position == 1 || position == 2) != (sample % 2 != 0));
				const auto start = std::chrono::steady_clock::now();
				for (int iteration = 0; iteration < iterations; ++iteration)
				{
					if (checked)
						candidate();
					else
						legacy();
				}
				const auto elapsed =
				    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start)
				        .count();
				std::cout << "BENCH," << rows << ',' << sample << ',' << position << ','
				          << (checked ? "checked_full_image" : "existing_map_copy") << ',' << iterations << ','
				          << elapsed << '\n';
			}
		}
		// Validate both ends of the full image outside the measured section.
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		ComPtr<ID3D11Texture1D> staging;
		require(SUCCEEDED(device->CreateTexture1D(&desc, nullptr, &staging)), "benchmark staging failed");
		context->CopyResource(staging.Get(), target.Get());
		D3D11_MAPPED_SUBRESOURCE mapped{};
		require(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "benchmark readback failed");
		const float *values = static_cast<const float *>(mapped.pData);
		const bool correct = values[0] == 1 && values[1] == 2 && values[2] == 3 && values[3] == 4 &&
		                     values[(rows - 1) * 4] == 1 && values[(rows - 1) * 4 + 3] == 4;
		context->Unmap(staging.Get(), 0);
		require(correct, "benchmark changed the required image");
	}
}

} // namespace

int main(int argc, char **argv)
{
	try
	{
		bool allow_hardware = false;
		int samples = 0;
		int iterations = 5000;
		for (int argument = 1; argument < argc; ++argument)
		{
			const std::string value(argv[argument]);
			if (value == "--allow-hardware")
				allow_hardware = true;
			else if (value == "--benchmark" && argument + 1 < argc)
				samples = std::stoi(argv[++argument]);
			else if (value == "--iterations" && argument + 1 < argc)
				iterations = std::stoi(argv[++argument]);
			else
				throw std::runtime_error(
				    "usage: IniParamsUploadTests [--allow-hardware] [--benchmark samples] [--iterations count]");
		}
		require(samples >= 0 && samples <= 1000 && iterations > 0 && iterations <= 1000000, "invalid benchmark count");
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
		std::cout << "ENV host="
		          << (GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version") ? "Wine" : "Windows")
		          << " pointer_bits=" << sizeof(void *) * 8 << '\n';
		ComPtr<IDXGIDevice> dxgi_device;
		ComPtr<IDXGIAdapter> adapter;
		DXGI_ADAPTER_DESC description{};
		if (SUCCEEDED(device.As(&dxgi_device)) && SUCCEEDED(dxgi_device->GetAdapter(&adapter)) &&
		    SUCCEEDED(adapter->GetDesc(&description)))
		{
			char name[512]{};
			if (WideCharToMultiByte(CP_UTF8, 0, description.Description, -1, name, static_cast<int>(sizeof(name)),
			                        nullptr, nullptr))
				std::cout << "ENV adapter=" << name << " vendor_id=" << description.VendorId << '\n';
		}
		exercise(device.Get(), context.Get());
		if (samples)
			benchmark_uploads(device.Get(), context.Get(), samples, iterations);
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
