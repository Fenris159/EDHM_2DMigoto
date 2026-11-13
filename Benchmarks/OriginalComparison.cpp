// Existing RuntimeTests benchmark workloads, adapted to the common public run()
// boundary so original 3Dmigoto and develop execute the same operation.
#include "CommandList.h"
#include "HackerContext.h"
#include "IniHandler.h"
#include "globals.h"
#include "input.h"
#include "profiling.h"
#include "D3DDeviceBoundary.h"

#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <d3d11sdklayers.h>
#include <wrl/client.h>

namespace
{
const std::wstring config_namespace = L"benchmark.ini";
void require(bool value, const char *message)
{
	if (!value)
		throw std::runtime_error(message);
}

struct BorrowedCustomResource : CustomResource
{
	~BorrowedCustomResource()
	{
		resource = nullptr;
		view = nullptr;
	}
};
struct D3DFixture
{
	HMODULE module = LoadLibraryExW(L"d3d11.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
	Microsoft::WRL::ComPtr<ID3D11Device1> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context;
	Microsoft::WRL::ComPtr<ID3D11InfoQueue> messages;
	std::unique_ptr<HackerContext> hacker_context;
	CommandListState state;

	explicit D3DFixture(bool validation = true)
	{
		require(module != nullptr, "could not load system D3D11");
		auto address = GetProcAddress(module, "D3D11CreateDevice");
		decltype(&D3D11CreateDevice) create = nullptr;
		static_assert(sizeof(create) == sizeof(address), "unexpected function pointer size");
		memcpy(&create, &address, sizeof(create));
		require(create != nullptr, "system D3D11CreateDevice is unavailable");
		Microsoft::WRL::ComPtr<ID3D11Device> base_device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> base_context;
		HRESULT result = create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, validation ? D3D11_CREATE_DEVICE_DEBUG : 0,
		                        nullptr, 0, D3D11_SDK_VERSION, &base_device, nullptr, &base_context);
		if (result == DXGI_ERROR_SDK_COMPONENT_MISSING)
		{
			std::cout << "LIMITATION D3D debug layer is unavailable\n";
			result = create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &base_device,
			                nullptr, &base_context);
		}
		require(SUCCEEDED(result), "could not create WARP device");
		require(SUCCEEDED(base_device.As(&device)), "D3D11.1 device unavailable");
		require(SUCCEEDED(base_context.As(&context)), "D3D11.1 context unavailable");
		state.mOrigDevice1 = device.Get();
		state.mOrigContext1 = context.Get();
		hacker_context = std::make_unique<HackerContext>(device.Get(), context.Get());
		state.mHackerContext = hacker_context.get();
		device.As(&messages);
	}

	void require_no_errors()
	{
		if (!messages)
			return;
		for (UINT64 index = 0; index < messages->GetNumStoredMessagesAllowedByRetrievalFilter(); ++index)
		{
			SIZE_T size = 0;
			require(SUCCEEDED(messages->GetMessage(index, nullptr, &size)), "could not query D3D diagnostics");
			std::vector<unsigned char> storage(size);
			auto *message = reinterpret_cast<D3D11_MESSAGE *>(storage.data());
			require(SUCCEEDED(messages->GetMessage(index, message, &size)), "could not read D3D diagnostic");
			if (message->Severity == D3D11_MESSAGE_SEVERITY_ERROR ||
			    message->Severity == D3D11_MESSAGE_SEVERITY_CORRUPTION)
				throw std::runtime_error(message->pDescription);
		}
	}

	~D3DFixture()
	{
		if (context)
			context->ClearState();
		hacker_context.reset();
		messages.Reset();
		context.Reset();
		device.Reset();
		if (module)
			FreeLibrary(module);
	}
};

void benchmark_command_lists(int samples)
{
	HackerContext context(nullptr, nullptr);
	CommandListState state;
	state.mHackerContext = &context;
	auto inserted =
	    command_list_globals.emplace(L"$benchmark", CommandListVariable(L"$benchmark", 0.0f, VariableFlags::NONE));
	auto &value = inserted.first->second;
	std::array<CommandList, 64> lists;
	std::vector<RunLinkedCommandList> runners;
	for (auto &list : lists)
	{
		auto assignment = std::make_shared<VariableAssignment>();
		assignment->var = &value;
		const std::wstring expression = L"$benchmark + 1";
		require(assignment->expression.parse(&expression, &config_namespace, nullptr), "benchmark expression rejected");
		list.commands.push_back(assignment);
		runners.emplace_back(&list);
	}
	constexpr int iterations = 500000;
	std::cout << "mode,sample,iterations,ns_per_run,registered_lists,registered_commands,value\n";
	for (const auto selected_mode : {Profiling::Mode::NONE, Profiling::Mode::SUMMARY,
	                                 Profiling::Mode::TOP_COMMAND_LISTS, Profiling::Mode::TOP_COMMANDS})
	{
		Profiling::mode = selected_mode;
		for (int sample = 0; sample < samples; ++sample)
		{
			Profiling::clear();
			value.fval = 0;
			const auto begin = std::chrono::steady_clock::now();
			for (int i = 0; i < iterations; ++i)
				runners[static_cast<size_t>(i) % runners.size()].run(&state);
			const auto elapsed = std::chrono::steady_clock::now() - begin;
			require(value.fval == 500000.0f, "command-list output changed");
			std::cout << static_cast<int>(selected_mode) << ',' << sample << ',' << iterations << ','
			          << std::chrono::duration<double, std::nano>(elapsed).count() / iterations << ','
			          << command_lists_profiling.size() << ',' << command_lists_cmd_profiling.size() << ','
			          << value.fval << '\n';
		}
	}
	Profiling::mode = Profiling::Mode::NONE;
	Profiling::clear();
}

void benchmark_resource_views(int samples)
{
	D3DFixture fixture(false);
	Microsoft::WRL::ComPtr<D3DDeviceBoundary> boundary;
	boundary.Attach(new D3DDeviceBoundary(fixture.device.Get()));
	fixture.state.mOrigDevice1 = boundary.Get();
	std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 32> buffers;
	std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 32> views;
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = 64;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
	view_desc.Format = DXGI_FORMAT_R32_FLOAT;
	view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	view_desc.Buffer.NumElements = 16;
	for (size_t i = 0; i < buffers.size(); ++i)
	{
		require(SUCCEEDED(fixture.device->CreateBuffer(&desc, nullptr, &buffers[i])), "benchmark buffer failed");
		require(SUCCEEDED(fixture.device->CreateShaderResourceView(buffers[i].Get(), &view_desc, &views[i])),
		        "benchmark source view failed");
	}
	BorrowedCustomResource source;
	source.substantiated = true;
	source.is_null = false;
	source.stride = 4;
	source.buf_size = 64;
	source.format = DXGI_FORMAT_R32_FLOAT;
	constexpr int iterations = 10000;
	std::cout << "workload,mode,sample,iterations,ns_per_run,create_calls,bound_resource_matches\n";
	for (const auto selected_mode : {Profiling::Mode::NONE, Profiling::Mode::SUMMARY})
	{
		Profiling::mode = selected_mode;
		for (const size_t resources : {size_t{1}, size_t{32}})
		{
			for (int sample = 0; sample < samples; ++sample)
			{
				ResourceCopyOperation operation;
				operation.options = ResourceCopyOptions::REFERENCE;
				operation.src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
#ifdef ORIGINAL_3DMIGOTO
				operation.src.custom_resource = &source;
#else
				operation.src.SetCustomResource(&source);
#endif
				operation.dst.type = ResourceCopyTargetType::SHADER_RESOURCE;
				operation.dst.shader_type = L'p';
				operation.dst.slot = 0;
				boundary->view_creations = 0;
				Profiling::clear();
				const auto begin = std::chrono::steady_clock::now();
				for (int i = 0; i < iterations; ++i)
				{
					const size_t index = static_cast<size_t>(i) % resources;
					// Borrow from the fixture's owning ComPtrs for this call.
					source.resource = buffers[index].Get();
					source.view = views[index].Get();
					operation.run(&fixture.state);
				}
				const auto elapsed = std::chrono::steady_clock::now() - begin;
				source.resource = nullptr;
				source.view = nullptr;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bound;
				fixture.context->PSGetShaderResources(0, 1, &bound);
				Microsoft::WRL::ComPtr<ID3D11Resource> bound_resource;
				if (bound)
					bound->GetResource(&bound_resource);
				const auto last = static_cast<size_t>(iterations - 1) % resources;
				require(bound_resource.Get() == buffers[last].Get(), "reference did not bind the selected buffer");
				std::cout << (resources == 1 ? "stable" : "churn") << ',' << static_cast<int>(selected_mode) << ','
				          << sample << ',' << iterations << ','
				          << std::chrono::duration<double, std::nano>(elapsed).count() / iterations << ','
				          << boundary->view_creations << ',' << (bound_resource.Get() == buffers[last].Get()) << '\n';
			}
		}
	}
	Profiling::mode = Profiling::Mode::NONE;
	Profiling::clear();
}
}
int main(int argc, char **argv)
{
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	InitializeCriticalSection(&G->mCriticalSection);
	InitializeCriticalSection(&G->mResourcesLock);
	InitializeCriticalSection(&resource_creation_mode_lock);
	G->hunting = 0;
#ifndef ORIGINAL_3DMIGOTO
	G->gShowWarnings = false;
#endif
	try
	{
		if (argc != 3)
			return 2;
		const int samples = std::stoi(argv[2]);
		if (samples < 1 || samples > 1000)
			return 2;
		if (std::string(argv[1]) == "lists")
			benchmark_command_lists(samples);
		else if (std::string(argv[1]) == "views")
			benchmark_resource_views(samples);
		else
			return 2;
	}
	catch (const std::exception &error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
