// A standalone, offscreen D3D11 caller. It never creates a swap chain or calls Present.
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <psapi.h>
#include <wrl/client.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;

namespace
{
void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

void checked(HRESULT result, const char *message)
{
	if (FAILED(result))
	{
		std::ostringstream error;
		error << message << ": HRESULT 0x" << std::hex << static_cast<unsigned>(result);
		throw std::runtime_error(error.str());
	}
}

template <class Function> Function symbol(HMODULE module, const char *name)
{
	const auto address = GetProcAddress(module, name);
	require(address != nullptr, name);
	Function function{};
	static_assert(sizeof(function) == sizeof(address));
	std::memcpy(&function, &address, sizeof(function));
	return function;
}

uint64_t shader_hash(ID3DBlob *shader)
{
	// The compared runtimes' shader_hash=3dmigoto contract (FNV with initial 0).
	uint64_t hash = 0;
	const auto *bytes = static_cast<const unsigned char *>(shader->GetBufferPointer());
	for (size_t i = 0; i < shader->GetBufferSize(); ++i)
	{
		hash *= 0x100000001b3ULL;
		hash ^= bytes[i];
	}
	return hash;
}

const char *shader_source = R"(
float4 VSMain(uint vertex : SV_VertexID) : SV_Position {
    float2 p[3] = { float2(-1,-1), float2(-1,3), float2(3,-1) };
    return float4(p[vertex], 0.5, 1);
}
float4 PSMain() : SV_Target { return float4(0,1,0,1); }
RWStructuredBuffer<uint> output : register(u0);
[numthreads(8,1,1)] void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x < 64) output[id.x] = id.x + 7;
}
)";

ComPtr<ID3DBlob> compile(HMODULE compiler, const char *entry, const char *target)
{
	auto call = symbol<decltype(&D3DCompile)>(compiler, "D3DCompile");
	ComPtr<ID3DBlob> code, errors;
	const HRESULT result = call(shader_source, std::strlen(shader_source), "offscreen-probe.hlsl", nullptr, nullptr,
	                            entry, target, D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &errors);
	if (FAILED(result) && errors)
		std::cerr.write(static_cast<const char *>(errors->GetBufferPointer()),
		                static_cast<std::streamsize>(errors->GetBufferSize()));
	checked(result, "compile offscreen shader");
	return code;
}

void emit(const char *metric, int sample, int iterations, double value, const char *unit)
{
	std::cout << "{\"metric\":\"" << metric << "\",\"sample\":" << sample << ",\"iterations\":" << iterations
	          << ",\"value\":" << std::setprecision(12) << value << ",\"unit\":\"" << unit << "\"}\n";
}

uint64_t private_bytes()
{
	PROCESS_MEMORY_COUNTERS_EX counters{};
	checked(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
	                             sizeof(counters))
	            ? S_OK
				: E_FAIL,
	        "query probe process memory");
	return counters.PrivateUsage;
}

void configuration(ID3DBlob *pixel, bool hud)
{
	std::cout << "[Logging]\nenabled=0\ncalls=0\ninput=0\ndebug=0\nunbuffered=0\n"
	             "[Hunting]\nhunting=0\n[System]\nload_library_redirect=0\nallow_platform_update=1\n"
	             "[Device]\nforce_stereo=0\n[Stereo]\nautomatic_mode=0\ncreate_profile=0\n"
	             "[Rendering]\nshader_hash=3dmigoto\n";
	if (hud)
		std::cout << "[ResourceHud]\ntype=Buffer\n[ShaderOverrideProbeHud]\nhash=" << std::hex << std::setw(16)
		          << std::setfill('0') << shader_hash(pixel) << std::dec
		          << "\nResourceHud=ref vs-t0\nps-t0=ref ResourceHud\n";
}

struct Probe
{
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	ComPtr<ID3D11Texture2D> color, depth;
	ComPtr<ID3D11Texture3D> volume;
	ComPtr<ID3D11RenderTargetView> rtv;
	ComPtr<ID3D11DepthStencilView> dsv;
	ComPtr<ID3D11ShaderResourceView> volume_view;
	ComPtr<ID3D11Buffer> output;
	ComPtr<ID3D11UnorderedAccessView> uav;
	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11PixelShader> ps;
	ComPtr<ID3D11ComputeShader> cs;
	ComPtr<ID3D11RasterizerState> rasterizer;
	ComPtr<ID3D11Query> idle;
	std::array<ComPtr<ID3D11Buffer>, 32> inputs;
	std::array<ComPtr<ID3D11ShaderResourceView>, 32> input_views;

	void initialize(HMODULE runtime, D3D_DRIVER_TYPE driver)
	{
		auto create = symbol<decltype(&D3D11CreateDevice)>(runtime, "D3D11CreateDevice");
		const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
		D3D_FEATURE_LEVEL selected{};
		checked(create(nullptr, driver, nullptr, 0, levels, 1, D3D11_SDK_VERSION, &device, &selected, &context),
		        "create offscreen D3D11 device");
		require(selected == D3D_FEATURE_LEVEL_11_0, "unexpected feature level");
	}

	void describe_adapter()
	{
		ComPtr<IDXGIDevice> dxgi;
		ComPtr<IDXGIAdapter> adapter;
		checked(device.As(&dxgi), "query DXGI device");
		checked(dxgi->GetAdapter(&adapter), "query adapter");
		DXGI_ADAPTER_DESC description{};
		checked(adapter->GetDesc(&description), "query adapter description");
		emit("adapter_vendor_id", 0, 1, description.VendorId, "identifier");
		emit("adapter_device_id", 0, 1, description.DeviceId, "identifier");
	}

	void setup(ID3DBlob *vertex, ID3DBlob *pixel, ID3DBlob *compute)
	{
		checked(device->CreateVertexShader(vertex->GetBufferPointer(), vertex->GetBufferSize(), nullptr, &vs),
		        "create VS");
		checked(device->CreatePixelShader(pixel->GetBufferPointer(), pixel->GetBufferSize(), nullptr, &ps),
		        "create PS");
		checked(device->CreateComputeShader(compute->GetBufferPointer(), compute->GetBufferSize(), nullptr, &cs),
		        "create CS");
		D3D11_TEXTURE2D_DESC texture{};
		texture.Width = texture.Height = 8;
		texture.MipLevels = texture.ArraySize = texture.SampleDesc.Count = 1;
		texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texture.BindFlags = D3D11_BIND_RENDER_TARGET;
		checked(device->CreateTexture2D(&texture, nullptr, &color), "create color surface");
		checked(device->CreateRenderTargetView(color.Get(), nullptr, &rtv), "create RTV");
		texture.Format = DXGI_FORMAT_D32_FLOAT;
		texture.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		checked(device->CreateTexture2D(&texture, nullptr, &depth), "create depth surface");
		checked(device->CreateDepthStencilView(depth.Get(), nullptr, &dsv), "create DSV");
		D3D11_TEXTURE3D_DESC volume_desc{};
		volume_desc.Width = volume_desc.Height = volume_desc.Depth = 4;
		volume_desc.MipLevels = 1;
		volume_desc.Format = DXGI_FORMAT_R32_UINT;
		volume_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		std::array<uint32_t, 64> sentinel;
		sentinel.fill(0x11223344U);
		D3D11_SUBRESOURCE_DATA volume_data{sentinel.data(), 16, 64};
		checked(device->CreateTexture3D(&volume_desc, &volume_data, &volume), "create untouched volume");
		checked(device->CreateShaderResourceView(volume.Get(), nullptr, &volume_view), "create volume SRV");
		D3D11_BUFFER_DESC buffer{};
		buffer.ByteWidth = 64;
		buffer.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		std::array<float, 16> values{};
		values.fill(11.0f);
		D3D11_SUBRESOURCE_DATA data{values.data(), 0, 0};
		D3D11_SHADER_RESOURCE_VIEW_DESC view{};
		view.Format = DXGI_FORMAT_R32_FLOAT;
		view.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		view.Buffer.NumElements = 16;
		for (size_t i = 0; i < inputs.size(); ++i)
		{
			checked(device->CreateBuffer(&buffer, &data, &inputs[i]), "create input buffer");
			checked(device->CreateShaderResourceView(inputs[i].Get(), &view, &input_views[i]), "create input SRV");
		}
		buffer.ByteWidth = 256;
		buffer.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		buffer.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer.StructureByteStride = 4;
		checked(device->CreateBuffer(&buffer, nullptr, &output), "create compute output");
		D3D11_UNORDERED_ACCESS_VIEW_DESC unordered{};
		unordered.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		unordered.Buffer.NumElements = 64;
		checked(device->CreateUnorderedAccessView(output.Get(), &unordered, &uav), "create UAV");
		D3D11_RASTERIZER_DESC raster{};
		raster.FillMode = D3D11_FILL_SOLID;
		raster.CullMode = D3D11_CULL_NONE;
		raster.DepthClipEnable = true;
		checked(device->CreateRasterizerState(&raster, &rasterizer), "create rasterizer");
		D3D11_QUERY_DESC query{D3D11_QUERY_EVENT, 0};
		checked(device->CreateQuery(&query, &idle), "create completion query");
	}

	void drain()
	{
		context->End(idle.Get());
		context->Flush();
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
		while (context->GetData(idle.Get(), nullptr, 0, 0) == S_FALSE)
		{
			require(std::chrono::steady_clock::now() < deadline, "offscreen completion timeout");
			Sleep(0);
		}
	}

	void render_and_compute(bool hud)
	{
		context->VSSetShader(vs.Get(), nullptr, 0);
		context->PSSetShader(ps.Get(), nullptr, 0);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->RSSetState(rasterizer.Get());
		D3D11_VIEWPORT viewport{0, 0, 8, 8, 0, 1};
		context->RSSetViewports(1, &viewport);
		auto *target = rtv.Get();
		context->OMSetRenderTargets(1, &target, dsv.Get());
		const float black[] = {0, 0, 0, 1};
		context->ClearRenderTargetView(rtv.Get(), black);
		context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1, 0);
		auto *source = input_views[1].Get();
		auto *pixel = input_views[0].Get();
		auto *sentinel = input_views[2].Get();
		context->VSSetShaderResources(0, 1, &source);
		context->PSSetShaderResources(0, 1, &pixel);
		context->PSSetShaderResources(1, 1, &sentinel);
		context->Draw(3, 0);
		ComPtr<ID3D11ShaderResourceView> bound, preserved;
		context->PSGetShaderResources(0, 1, &bound);
		context->PSGetShaderResources(1, 1, &preserved);
		if (hud)
		{
			require(bound != nullptr, "configured HUD reference did not bind a view");
			ComPtr<ID3D11Resource> resource;
			bound->GetResource(&resource);
			D3D11_SHADER_RESOURCE_VIEW_DESC description{};
			bound->GetDesc(&description);
			require(resource.Get() == inputs[1].Get() && description.Format == DXGI_FORMAT_R32_FLOAT &&
			            description.ViewDimension == D3D11_SRV_DIMENSION_BUFFER &&
			            description.Buffer.FirstElement == 0 && description.Buffer.NumElements == 16,
			        "configured HUD reference changed the selected buffer or range");
		}
		else
			require(bound.Get() == pixel, "passive pixel binding changed");
		require(preserved.Get() == sentinel, "untargeted PS slot changed");
		ComPtr<ID3D11VertexShader> actual_vs;
		context->VSGetShader(&actual_vs, nullptr, nullptr);
		require(actual_vs.Get() == vs.Get(), "untargeted vertex shader changed");
		context->CSSetShader(cs.Get(), nullptr, 0);
		auto *write = uav.Get();
		context->CSSetUnorderedAccessViews(0, 1, &write, nullptr);
		context->Dispatch(8, 1, 1);
		ID3D11UnorderedAccessView *empty = nullptr;
		context->CSSetUnorderedAccessViews(0, 1, &empty, nullptr);
		drain();
		emit("targeted_and_untargeted_state", 0, 1, 1, "pass");
	}

	template <class Action> void measure(const char *name, int sample, int count, Action action)
	{
		drain();
		const auto start = std::chrono::steady_clock::now();
		for (int i = 0; i < count; ++i)
			action(i);
		const auto elapsed = std::chrono::steady_clock::now() - start;
		emit(name, sample, count, std::chrono::duration<double, std::nano>(elapsed).count() / count,
		     "ns_per_operation");
		drain();
	}

	void measure_calls(int samples, int iterations, bool hud)
	{
		for (int sample = 0; sample < samples; ++sample)
		{
			for (const size_t active : {size_t{1}, size_t{32}})
			{
				measure(active == 1 ? "bind_stable" : "bind_churn", sample, iterations,
				        [&](int i)
				        {
					        auto *view = input_views[static_cast<size_t>(i) % active].Get();
					        context->PSSetShaderResources(0, 1, &view);
				        });
				if (hud)
					measure(active == 1 ? "hud_reference_stable" : "hud_reference_churn", sample, iterations,
					        [&](int i)
					        {
						        auto *view = input_views[static_cast<size_t>(i) % active].Get();
						        context->VSSetShaderResources(0, 1, &view);
						        context->Draw(3, 0);
					        });
			}
			D3D11_BUFFER_DESC desc{};
			desc.ByteWidth = 64;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			measure("create_release_buffer", sample, 128,
			        [&](int)
			        {
				        ComPtr<ID3D11Buffer> temporary;
				        checked(device->CreateBuffer(&desc, nullptr, &temporary), "create transient buffer");
			        });
		}
	}

	void verify_outputs(int samples)
	{
		context->OMSetRenderTargets(0, nullptr, nullptr);
		D3D11_TEXTURE2D_DESC desc{};
		color->GetDesc(&desc);
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		ComPtr<ID3D11Texture2D> color_read, depth_read;
		checked(device->CreateTexture2D(&desc, nullptr, &color_read), "create color validation staging");
		desc.Format = DXGI_FORMAT_D32_FLOAT;
		checked(device->CreateTexture2D(&desc, nullptr, &depth_read), "create depth validation staging");
		context->CopyResource(color_read.Get(), color.Get());
		context->CopyResource(depth_read.Get(), depth.Get());
		D3D11_MAPPED_SUBRESOURCE mapped{};
		checked(context->Map(color_read.Get(), 0, D3D11_MAP_READ, 0, &mapped), "read color output");
		bool color_ok = true;
		for (size_t row = 0; row < 8; ++row)
			for (size_t column = 0; column < 8; ++column)
			{
				uint32_t pixel{};
				std::memcpy(&pixel, static_cast<const char *>(mapped.pData) + row * mapped.RowPitch + column * 4, 4);
				color_ok = color_ok && pixel == 0xff00ff00U;
			}
		context->Unmap(color_read.Get(), 0);
		require(color_ok, "offscreen color differs from expected opaque green");
		checked(context->Map(depth_read.Get(), 0, D3D11_MAP_READ, 0, &mapped), "read depth output");
		bool depth_ok = true;
		for (size_t row = 0; row < 8; ++row)
			for (size_t column = 0; column < 8; ++column)
			{
				float value{};
				std::memcpy(&value, static_cast<const char *>(mapped.pData) + row * mapped.RowPitch + column * 4, 4);
				depth_ok = depth_ok && value == 0.5f;
			}
		context->Unmap(depth_read.Get(), 0);
		require(depth_ok, "offscreen depth differs from expected 0.5");
		D3D11_TEXTURE3D_DESC volume_desc{};
		volume->GetDesc(&volume_desc);
		volume_desc.Usage = D3D11_USAGE_STAGING;
		volume_desc.BindFlags = 0;
		volume_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		ComPtr<ID3D11Texture3D> volume_read;
		checked(device->CreateTexture3D(&volume_desc, nullptr, &volume_read), "create volume validation staging");
		context->CopyResource(volume_read.Get(), volume.Get());
		checked(context->Map(volume_read.Get(), 0, D3D11_MAP_READ, 0, &mapped), "read untouched volume");
		bool volume_ok = true;
		for (size_t slice = 0; slice < 4; ++slice)
			for (size_t row = 0; row < 4; ++row)
				for (size_t column = 0; column < 4; ++column)
				{
					uint32_t value{};
					std::memcpy(&value,
					            static_cast<const char *>(mapped.pData) + slice * mapped.DepthPitch +
					                row * mapped.RowPitch + column * 4,
					            4);
					volume_ok = volume_ok && value == 0x11223344U;
				}
		context->Unmap(volume_read.Get(), 0);
		require(volume_ok, "untargeted volume changed");
		D3D11_BUFFER_DESC buffer{};
		output->GetDesc(&buffer);
		buffer.Usage = D3D11_USAGE_STAGING;
		buffer.BindFlags = buffer.MiscFlags = buffer.StructureByteStride = 0;
		buffer.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		ComPtr<ID3D11Buffer> output_read;
		checked(device->CreateBuffer(&buffer, nullptr, &output_read), "create compute validation staging");
		context->CopyResource(output_read.Get(), output.Get());
		checked(context->Map(output_read.Get(), 0, D3D11_MAP_READ, 0, &mapped), "read compute output");
		const uint32_t expected[] = {7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
		                             23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
		                             39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54,
		                             55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70};
		const bool compute_ok = std::memcmp(mapped.pData, expected, sizeof(expected)) == 0;
		context->Unmap(output_read.Get(), 0);
		require(compute_ok, "untargeted compute output changed");
		for (int sample = 0; sample < samples; ++sample)
			measure("copy_and_readback_256_bytes", sample, 64,
			        [&](int)
			        {
				        context->CopyResource(output_read.Get(), output.Get());
				        checked(context->Map(output_read.Get(), 0, D3D11_MAP_READ, 0, &mapped),
						        "benchmark compute readback");
				        const bool matches = std::memcmp(mapped.pData, expected, sizeof(expected)) == 0;
				        context->Unmap(output_read.Get(), 0);
				        require(matches, "readback changed compute values");
			        });
		emit("color_depth_volume_compute", 0, 1, 1, "pass");
		emit("probe_validation_copy_calls", 0, 1, 4, "calls");
		emit("probe_validation_map_calls", 0, 1, 4, "calls");
		emit("probe_measured_copy_and_map_calls", 0, 1, samples * 64, "calls_each");
		emit("fixture_primary_resource_payload", 0, 1, 3072, "logical_bytes");
		emit("probe_validation_staging_payload", 0, 1, 1024, "logical_bytes");
	}
};
}

int wmain(int argc, wchar_t **argv)
{
	std::cout << std::unitbuf;
	try
	{
		require(argc == 7 || (argc == 4 && std::wstring(argv[1]) == L"--config"), "invalid probe arguments");
		const bool config = std::wstring(argv[1]) == L"--config";
		HMODULE compiler = LoadLibraryExW(argv[2], nullptr, 0);
		require(compiler != nullptr, "load pinned shader compiler");
		auto vertex = compile(compiler, "VSMain", "vs_5_0");
		auto pixel = compile(compiler, "PSMain", "ps_5_0");
		auto compute = compile(compiler, "CSMain", "cs_5_0");
		const bool hud = std::wstring(argv[config ? 3 : 4]) == L"hud";
		if (config)
		{
			configuration(pixel.Get(), hud);
			return 0;
		}
		const int samples = std::stoi(argv[5]), iterations = std::stoi(argv[6]);
		require(samples > 0 && samples <= 100 && iterations > 0 && iterations <= 100000, "invalid probe limits");
		const auto before = private_bytes();
		const auto begin = std::chrono::steady_clock::now();
		HMODULE runtime = std::wstring(argv[1]) == L"system"
		                      ? LoadLibraryExW(L"d3d11.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)
		                      : LoadLibraryExW(argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
		if (!runtime)
			checked(HRESULT_FROM_WIN32(GetLastError()), "load selected D3D11 runtime");
		require(runtime != nullptr, "load selected D3D11 runtime");
		{
			Probe probe;
			probe.initialize(runtime,
			                 std::wstring(argv[3]) == L"warp" ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE);
			emit("runtime_load_and_device", 0, 1,
			     std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count(), "ms");
			probe.describe_adapter();
			emit("process_private_before_runtime", 0, 1, static_cast<double>(before), "bytes");
			emit("process_private_after_device", 0, 1, static_cast<double>(private_bytes()), "bytes");
			probe.setup(vertex.Get(), pixel.Get(), compute.Get());
			probe.render_and_compute(hud && std::wstring(argv[1]) != L"system");
			probe.measure_calls(samples, iterations, hud && std::wstring(argv[1]) != L"system");
			probe.verify_outputs(samples);
			emit("process_private_after_workload", 0, 1, static_cast<double>(private_bytes()), "bytes");
			probe.context->ClearState();
			probe.drain();
		}
		// Diagnostic-only: unload the outer wrapper while the lower tracer is
		// still alive, after releasing the caller's COM objects.
		if (GetEnvironmentVariableW(L"EDHM_PROBE_UNLOAD_RUNTIME", nullptr, 0))
			checked(FreeLibrary(runtime) ? S_OK : E_FAIL, "unload diagnostic runtime");
	}
	catch (const std::exception &error)
	{
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
	}
	return 0;
}
