#include "../DirectX11/CommandList.h"
#include "../DirectX11/HackerContext.h"
#include "../DirectX11/FrameAnalysis.h"
#include "../DirectX11/IniHandler.h"
#include "../DirectX11/globals.h"
#include "D3DDeviceBoundary.h"
#include "D3DMapBoundary.h"
#include "../DirectX11/Override.h"
#include "ImportBoundary.h"
#include "../DirectX11/input.h"
#include "../DirectX11/profiling.h"

#include <array>
#include <atomic>
#include <chrono>
#include <clocale>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>
#include <d3d11sdklayers.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace
{
const std::wstring config_namespace = L"test.ini";

void require(bool result, const char *message)
{
	if (!result)
		throw std::runtime_error(message);
}

float evaluate(const wchar_t *text)
{
	CommandListExpression expression;
	const std::wstring input(text);
	float result = 0;
	require(expression.parse(&input, nullptr, nullptr), "valid expression was rejected");
	require(expression.static_evaluate(&result), "constant expression did not evaluate");
	return result;
}

void rejects_empty_groups()
{
	for (const wchar_t *text : {L"()", L"1 + (())"})
	{
		CommandListExpression expression;
		const std::wstring input(text);
		require(!expression.parse(&input, nullptr, nullptr), "empty group was accepted");
	}
	require(evaluate(L"(2 + 3) * 4") == 20.0f, "grouping changed arithmetic precedence");
}

void accepts_leading_dot_numbers()
{
	require(evaluate(L".125") == 0.125f, "leading-dot literal changed value");
	require(evaluate(L"-.125") == -0.125f, "negative leading-dot literal changed value");
	require(evaluate(L"2-.125") == 1.875f, "leading-dot literal broke subtraction");
	for (const wchar_t *text : {L".", L".1.2", L"1..2"})
	{
		CommandListExpression expression;
		const std::wstring input(text);
		require(!expression.parse(&input, nullptr, nullptr), "malformed numeric input was accepted");
	}
}

void resets_only_the_requested_pool_proxy()
{
	auto &source = customResourcePools[L"poolsource"];
	auto &proxy = customResourcePools[L"poolproxy"];
	auto &unrelated = customResourcePools[L"poolunrelated"];
	source.name = L"poolsource";
	proxy.name = L"poolproxy";
	unrelated.name = L"poolunrelated";
	source.variable_template = std::make_unique<CommandListVariable>(L"$source", 9.0f, VariableFlags::NONE);
	proxy.variable_template = std::make_unique<CommandListVariable>(L"$proxy", 3.0f, VariableFlags::NONE);
	unrelated.variable_template = std::make_unique<CommandListVariable>(L"$other", 5.0f, VariableFlags::NONE);
	source.Initialize(2);
	proxy.Initialize(2);
	unrelated.Initialize(2);
	require(proxy.SetSourcePool(&source), "could not establish pool proxy");
	CommandList commands;
	std::wstring value = L"null";
	require(ParseCommandListResourceCopyTargetDirective(L"CommandListTest", L"poolproxy", &value, &commands,
	                                                    &config_namespace),
	        "pool proxy reset was rejected");
	CommandListState state;
	HackerContext context(nullptr, nullptr);
	state.mHackerContext = &context;
	for (const auto &command : commands.commands)
		command->run(&state);
	require(proxy.GetVariable(0, false, false, false)->fval == 3.0f, "proxy did not return to its own variables");
	require(source.GetVariable(0, false, false, false)->fval == 9.0f, "proxy reset altered its source");
	require(unrelated.GetVariable(0, false, false, false)->fval == 5.0f, "proxy reset altered an unrelated pool");
	commands.clear();
	customResourcePools.clear();
}

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

void final_resource_release_removes_live_metadata()
{
	D3DFixture fixture;
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = 16;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, nullptr, &buffer)), "tracked buffer creation failed");
	require(SUCCEEDED(ResourceReleaseTracker::Attach(buffer.Get())), "resource tracker attachment failed");
	auto *key = buffer.Get();
	G->mResources[key].hash = 0x2468;
	D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
	view_desc.Format = DXGI_FORMAT_R32_FLOAT;
	view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	view_desc.Buffer.NumElements = 4;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
	require(SUCCEEDED(fixture.device->CreateShaderResourceView(buffer.Get(), &view_desc, &view)),
	        "retained buffer view creation failed");
	buffer.Reset();
	require(G->mResources.at(key).hash == 0x2468, "live view lost its resource metadata");
	view.Reset();
	fixture.context->Flush();
	require(G->mResources.count(key) == 0, "final resource release retained stale metadata");
	fixture.require_no_errors();
}

void resource_release_survives_registry_destruction()
{
	D3DFixture fixture;
	struct RegistryOwner
	{
		Globals *previous = G;
		void *storage = VirtualAlloc(nullptr, sizeof(Globals), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		Globals *owner = nullptr;
		explicit RegistryOwner()
		{
			require(storage != nullptr, "registry allocation failed");
			owner = new (storage) Globals();
			InitializeCriticalSection(&owner->mResourcesLock);
			G = owner;
		}
		RegistryOwner(const RegistryOwner &) = delete;
		RegistryOwner &operator=(const RegistryOwner &) = delete;
		RegistryOwner(RegistryOwner &&) = delete;
		RegistryOwner &operator=(RegistryOwner &&) = delete;
		void retire()
		{
			owner->~Globals();
			DeleteCriticalSection(&owner->mResourcesLock);
			owner = nullptr;
			DWORD previous_protection;
			// Make every post-destruction read, including a lock or G flag,
			// an observable failure instead of relying on freed heap contents.
			require(VirtualProtect(storage, sizeof(Globals), PAGE_NOACCESS, &previous_protection) != FALSE,
			        "retired registry protection failed");
		}
		~RegistryOwner()
		{
			if (owner)
			{
				owner->~Globals();
				DeleteCriticalSection(&owner->mResourcesLock);
			}
			G = previous;
			VirtualFree(storage, 0, MEM_RELEASE);
		}
	} registry;
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = 16;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, nullptr, &buffer)), "late buffer creation failed");
	require(SUCCEEDED(ResourceReleaseTracker::Attach(buffer.Get())), "late tracker attachment failed");
	G->mResources[buffer.Get()].hash = 0x1357;
	D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
	view_desc.Format = DXGI_FORMAT_R32_FLOAT;
	view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	view_desc.Buffer.NumElements = 4;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
	require(SUCCEEDED(fixture.device->CreateShaderResourceView(buffer.Get(), &view_desc, &view)),
	        "late buffer view creation failed");
	registry.retire();
	buffer.Reset();
	view.Reset();
	fixture.context->Flush();
	fixture.require_no_errors();
}

struct IniParamsFixture
{
	D3DFixture d3d;
	struct Rows
	{
		std::vector<DirectX::XMFLOAT4> saved = std::move(G->iniParams);
		int reserved = G->iniParamsReserved;
		unsigned updates = Profiling::iniparams_updates;
		Rows()
		{
			G->iniParams.assign(1, DirectX::XMFLOAT4{0, 0, 0, 0});
			G->iniParamsReserved = 1;
		}
		~Rows()
		{
			G->iniParams = std::move(saved);
			G->iniParamsReserved = reserved;
			Profiling::iniparams_updates = updates;
		}
	} rows;
	HackerDevice device;
	struct Resources
	{
		HackerDevice &device;
		~Resources()
		{
			if (device.mIniResourceView)
				device.mIniResourceView->Release();
			if (device.mIniTexture)
				device.mIniTexture->Release();
			device.mIniResourceView = nullptr;
			device.mIniTexture = nullptr;
		}
	} resources;

	explicit IniParamsFixture(bool validation = true)
	    : d3d(validation), device(d3d.device.Get(), d3d.context.Get()), resources{device}
	{
		device.SetHackerContext(d3d.hacker_context.get());
		d3d.hacker_context->SetHackerDevice(&device, false);
		require(SUCCEEDED(device.CreateIniParamResources()), "IniParams resource creation failed");
	}
	~IniParamsFixture()
	{
		d3d.context->ClearState();
		d3d.hacker_context->SetHackerDevice(nullptr, false);
	}
	std::array<float, 4> read()
	{
		D3D11_TEXTURE1D_DESC desc{};
		device.mIniTexture->GetDesc(&desc);
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		Microsoft::WRL::ComPtr<ID3D11Texture1D> staging;
		require(SUCCEEDED(d3d.device->CreateTexture1D(&desc, nullptr, &staging)), "IniParams staging failed");
		d3d.context->CopyResource(staging.Get(), device.mIniTexture);
		D3D11_MAPPED_SUBRESOURCE mapped{};
		require(SUCCEEDED(d3d.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "IniParams readback failed");
		std::array<float, 4> image{};
		memcpy(image.data(), mapped.pData, sizeof(image));
		d3d.context->Unmap(staging.Get(), 0);
		return image;
	}
};

ULONGLONG WINAPI publication_tick()
{
	return 100;
}

void final_transition_upload_retries_after_retirement()
{
	IniParamsFixture fixture;
	OverrideTransition transition;
	OverrideTransitionParam value;
	value.start = 0;
	value.target = 4;
	value.activation_time = 0;
	value.time = 100;
	transition.params.emplace(OverrideParam(0, &DirectX::XMFLOAT4::x), value);
	ImportBoundary clock(GetModuleHandleW(nullptr), "GetTickCount64", &publication_tick);
	{
		D3DMapBoundary boundary(fixture.d3d.context.Get(), fixture.device.mIniTexture);
		boundary.failures = 1;
		transition.UpdateTransitions(&fixture.device);
		require(fixture.read() == std::array<float, 4>{0, 0, 0, 0}, "failed final upload changed GPU contents");
	}
	transition.UpdateTransitions(&fixture.device);
	require(fixture.read() == std::array<float, 4>{4, 0, 0, 0}, "retired transition lost its final GPU value");
	fixture.d3d.require_no_errors();
}

void command_upload_retries_after_its_state_retires()
{
	IniParamsFixture fixture;
	for (bool empty_retry : {true, false})
	{
		CommandList commands;
		auto assignment = std::make_shared<ParamOverride>();
		assignment->param_idx = 0;
		assignment->param_component = &DirectX::XMFLOAT4::x;
		const std::wstring value = empty_retry ? L"2" : L"4";
		require(assignment->expression.parse(&value, nullptr, nullptr), "IniParams assignment was rejected");
		commands.commands.push_back(assignment);
		{
			D3DMapBoundary boundary(fixture.d3d.context.Get(), fixture.device.mIniTexture);
			boundary.failures = 1;
			RunCommandList(&fixture.device, fixture.d3d.hacker_context.get(), &commands, nullptr, false);
			const std::array<float, 4> previous =
			    empty_retry ? std::array<float, 4>{0, 0, 0, 0} : std::array<float, 4>{2, 0, 0, 0};
			require(fixture.read() == previous, "failed command upload changed GPU contents");
		}
		CommandList retry;
		CommandListVariable local(L"$retry", 0, VariableFlags::NONE);
		if (!empty_retry)
		{
			auto non_gpu_assignment = std::make_shared<VariableAssignment>();
			non_gpu_assignment->var = &local;
			const std::wstring one = L"1";
			require(non_gpu_assignment->expression.parse(&one, nullptr, nullptr), "retry command was rejected");
			retry.commands.push_back(non_gpu_assignment);
		}
		RunCommandList(&fixture.device, fixture.d3d.hacker_context.get(), &retry, nullptr, false);
		const std::array<float, 4> expected =
		    empty_retry ? std::array<float, 4>{2, 0, 0, 0} : std::array<float, 4>{4, 0, 0, 0};
		require(fixture.read() == expected, "expired command state lost its pending upload");
		if (!empty_retry)
			require(local.fval == 1, "nonempty retry point skipped its ordinary command");
	}
	fixture.d3d.require_no_errors();
}

void deferred_recording_does_not_acknowledge_immediate_publication()
{
	IniParamsFixture fixture;
	G->iniParams[0].x = 4;
	{
		D3DMapBoundary failure(fixture.d3d.context.Get(), fixture.device.mIniTexture);
		failure.failures = 1;
		require(!fixture.d3d.hacker_context->PublishIniParams(&fixture.device, true),
		        "failed immediate publication was acknowledged");
	}
	Microsoft::WRL::ComPtr<ID3D11DeviceContext1> raw_deferred;
	require(SUCCEEDED(fixture.d3d.device->CreateDeferredContext1(0, &raw_deferred)), "deferred context failed");
	HackerContext deferred(fixture.d3d.device.Get(), raw_deferred.Get());
	deferred.SetHackerDevice(&fixture.device, false);
	require(deferred.PublishIniParams(&fixture.device, true), "deferred publication failed");
	Microsoft::WRL::ComPtr<ID3D11CommandList> recorded;
	require(SUCCEEDED(raw_deferred->FinishCommandList(FALSE, &recorded)), "deferred recording failed");
	require(fixture.read() == std::array<float, 4>{0, 0, 0, 0}, "recording changed the immediate image");
	G->iniParams[0].x = 7;
	require(fixture.d3d.hacker_context->PublishIniParams(&fixture.device, false), "immediate retry failed");
	require(fixture.read() == std::array<float, 4>{7, 0, 0, 0}, "deferred recording acknowledged immediate work");
	fixture.d3d.context->ExecuteCommandList(recorded.Get(), FALSE);
	require(fixture.read() == std::array<float, 4>{4, 0, 0, 0}, "deferred execution lost its recorded image");
	fixture.d3d.require_no_errors();
}

void recreates_structured_data_as_a_constant_buffer()
{
	D3DFixture fixture;
	Microsoft::WRL::ComPtr<D3DDeviceBoundary> boundary;
	boundary.Attach(new D3DDeviceBoundary(fixture.device.Get()));
	fixture.state.mOrigDevice1 = boundary.Get();
	const float data[] = {2.0f, 4.0f, 6.0f, 8.0f};
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(data);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = 8;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = data;
	Microsoft::WRL::ComPtr<ID3D11Buffer> source;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, &initial, &source)), "structured source creation failed");
	ResourceCopyOperation operation;
	operation.options = ResourceCopyOptions::COPY;
	require(operation.dst.ParseTarget(L"vs-cb0", false, nullptr, nullptr), "constant buffer target was rejected");
	operation.CopyResourceToResource(&fixture.state, source.Get(), nullptr, 8, 0, DXGI_FORMAT_UNKNOWN, 16);
	require(boundary->buffer_creations == 1, "copy did not submit exactly one buffer creation");
	require(boundary->last_buffer_description.StructureByteStride == 0,
	        "runtime submitted a stale structured stride to D3D");
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant;
	fixture.context->VSGetConstantBuffers(0, 1, &constant);
	require(constant != nullptr, "constant buffer was not bound");
	constant->GetDesc(&desc);
	require(desc.ByteWidth == 16, "copy changed constant buffer size");
	require(desc.MiscFlags == 0, "copy retained structured buffer flags");
	require(desc.StructureByteStride == 0, "constant buffer retained source element stride");
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	Microsoft::WRL::ComPtr<ID3D11Buffer> staging;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, nullptr, &staging)), "staging buffer creation failed");
	fixture.context->CopyResource(staging.Get(), constant.Get());
	D3D11_MAPPED_SUBRESOURCE mapped{};
	require(SUCCEEDED(fixture.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "readback failed");
	float result[4]{};
	memcpy(result, mapped.pData, sizeof(result));
	fixture.context->Unmap(staging.Get(), 0);
	require(result[0] == 2.0f && result[1] == 4.0f && result[2] == 6.0f && result[3] == 8.0f,
	        "constant buffer copy changed the data");
	fixture.require_no_errors();
}

void preserves_store_values_when_readback_allocation_fails()
{
	D3DFixture fixture;
	Microsoft::WRL::ComPtr<D3DDeviceBoundary> boundary;
	boundary.Attach(new D3DDeviceBoundary(fixture.device.Get()));
	boundary->fail_staging = true;
	HackerContext context(boundary.Get(), fixture.context.Get());
	fixture.state.mOrigDevice1 = boundary.Get();
	fixture.state.mHackerContext = &context;
	const float values[] = {7, 8, 9, 10};
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(values);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = values;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, &initial, &buffer)), "readback source creation failed");
	CustomResource source;
	source.resource = buffer.Detach();
	source.device = boundary.Get();
	source.is_null = false;
	CommandListVariable value(L"$result", 42.0f, VariableFlags::NONE);
	StoreCommand command;
	command.src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
	command.src.SetCustomResource(&source);
	command.var = &value;
	command.offset_expression = std::make_unique<CommandListExpression>();
	const std::wstring index = L"0";
	require(command.offset_expression->parse(&index, &config_namespace, nullptr), "store index was rejected");
	command.run(&fixture.state);
	require(boundary->buffer_creations == 1, "readback failure was not exercised");
	require(value.fval == 42.0f, "failed readback allocation overwrote the previous value");
	fixture.require_no_errors();
}

struct ConfigFixture
{
	~ConfigFixture()
	{
		ClearDeferredBindFlags();
		customResourcePools.clear();
		customResources.clear();
		persistent_variables.clear();
		command_list_globals.clear();
	}
};

void parse_copy(CommandList &commands, const wchar_t *destination, const wchar_t *source)
{
	std::wstring value(source);
	require(ParseCommandListResourceCopyTargetDirective(L"CommandListTest", destination, &value, &commands,
	                                                    &config_namespace),
	        "resource command was rejected");
}

CustomResourcePool &make_pool(const wchar_t *name, float default_value)
{
	auto &pool = customResourcePools[name];
	pool.name = name;
	pool.resource_template = &customResources[pool.name + L"_template"];
	pool.resource_template->pool = &pool;
	pool.resource_template->pool_index = -1;
	pool.variable_template =
	    std::make_unique<CommandListVariable>(L"$" + pool.name, default_value, VariableFlags::NONE);
	pool.Initialize(2);
	return pool;
}

void resolves_reference_chains_in_any_order()
{
	D3DFixture fixture;
	for (bool constant_buffer : {false, true})
	{
		for (bool reverse : {false, true})
		{
			ConfigFixture resources;
			CommandList commands;
			auto &source = customResources[L"resourcesource"];
			customResources[L"resourcealias"];
			customResources[L"resourceslot"];
			const auto expected = constant_buffer ? D3D11_BIND_CONSTANT_BUFFER : D3D11_BIND_SHADER_RESOURCE;
			if (constant_buffer)
				source.bind_flags = D3D11_BIND_CONSTANT_BUFFER;
			const wchar_t *slot = constant_buffer ? L"vs-cb3" : L"ps-t3";
			if (reverse)
			{
				parse_copy(commands, slot, L"ref resourceslot");
				parse_copy(commands, L"resourceslot", L"ref resourcealias");
				parse_copy(commands, L"resourcealias", L"ref resourcesource");
			}
			else
			{
				parse_copy(commands, L"resourcealias", L"ref resourcesource");
				parse_copy(commands, L"resourceslot", L"ref resourcealias");
				parse_copy(commands, slot, L"ref resourceslot");
			}
			PropagateDeferredBindFlags();
			for (const wchar_t *name : {L"resourcesource", L"resourcealias", L"resourceslot"})
				require(customResources.at(name).bind_flags == expected,
				        "reference chain lost the final usage across order/reload");
			source.override_type = CustomResourceType::BUFFER;
			source.override_byte_width = 16;
			source.override_format = DXGI_FORMAT_R32_FLOAT;
			source.Substantiate(fixture.device.Get(), static_cast<D3D11_BIND_FLAG>(0),
			                    static_cast<D3D11_RESOURCE_MISC_FLAG>(0));
			require(source.resource != nullptr, "reference chain did not produce a usable source buffer");
			if (constant_buffer)
			{
				Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
				require(SUCCEEDED(source.resource->QueryInterface(IID_PPV_ARGS(&buffer))), "CB source is not a buffer");
				auto *binding = buffer.Get();
				fixture.context->VSSetConstantBuffers(3, 1, &binding);
				Microsoft::WRL::ComPtr<ID3D11Buffer> bound;
				fixture.context->VSGetConstantBuffers(3, 1, &bound);
				require(bound == buffer, "reference metadata prevented actual CB binding");
				fixture.context->ClearState();
			}
			else
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
				desc.Format = DXGI_FORMAT_R32_FLOAT;
				desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
				desc.Buffer.NumElements = 4;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
				require(SUCCEEDED(fixture.device->CreateShaderResourceView(source.resource, &desc, &view)),
				        "reference order prevented actual SRV creation");
			}
		}
	}
	fixture.require_no_errors();
}

void terminates_incompatible_reference_cycles()
{
	ConfigFixture resources;
	CommandList commands;
	auto &a = customResources[L"resourcea"];
	auto &b = customResources[L"resourceb"];
	parse_copy(commands, L"resourcea", L"ref resourceb");
	parse_copy(commands, L"resourceb", L"ref resourcea");
	parse_copy(commands, L"ps-t0", L"ref resourcea");
	parse_copy(commands, L"vs-cb0", L"ref resourceb");
	a.misc_flags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	PropagateDeferredBindFlags();
	require(a.bind_flags == D3D11_BIND_SHADER_RESOURCE, "cycle corrupted SRV usage");
	require(b.bind_flags == D3D11_BIND_CONSTANT_BUFFER, "cycle corrupted constant-buffer usage");
	require(b.misc_flags == 0, "rejected incompatible reference modified constant-buffer metadata");
}

void metadata_queries_do_not_extend_pool_lifetime()
{
	D3DFixture fixture;
	ConfigFixture resources;
	auto &pool = make_pool(L"poolmetadata", 3.0f);
	pool.expiration_timeout_frames = 5;
	pool.read_refreshes_expiration = true;
	pool.Initialize(2);
	G->frame_no = 10;
	pool.GetVariable(0, false, true, true)->fval = 9.0f;
	customResources[L"resourcemissing"];
	CommandList commands;
	parse_copy(commands, L"poolmetadata[0]", L"ref unless_null resourcemissing");
	G->frame_no = 14;
	ResourceCopyTarget metadata;
	require(metadata.ParseTarget(L"poolmetadata[0]", true, &config_namespace, nullptr), "metadata target rejected");
	require(metadata.BindFlags(&fixture.state) == 0, "empty pool metadata gained binding requirements");
	for (const auto &command : commands.commands)
		command->run(&fixture.state);
	G->frame_no = 17;
	pool.read_refreshes_expiration = false;
	require(pool.GetVariable(0, false, true, false)->fval == 3.0f,
	        "cancelled unless_null copy refreshed an untouched pool element");
	auto &static_pool = make_pool(L"poolstatic", 1.0f);
	static_pool.index_type = PoolIndexType::STATIC;
	static_pool.GetResource(0, false, false, true)->bind_flags = D3D11_BIND_VERTEX_BUFFER;
	ResourceCopyTarget target;
	require(target.ParseTarget(L"poolstatic[0]", true, &config_namespace, nullptr), "static pool target was rejected");
	require(target.BindFlags(&fixture.state) == D3D11_BIND_VERTEX_BUFFER,
	        "metadata query replaced a statically resolved element with its template");
	fixture.require_no_errors();
}

void skipped_copies_do_not_evaluate_nested_destination_indexes()
{
	D3DFixture fixture;
	ConfigFixture resources;
	for (bool cached : {false, true})
	{
		const wchar_t *pool_name = cached ? L"poolcachednested" : L"poolfreshnested";
		auto &pool = make_pool(pool_name, 3.0f);
		pool.expiration_timeout_frames = 5;
		pool.read_refreshes_expiration = true;
		pool.Initialize(2);
		G->frame_no = 10;
		pool.GetVariable(0, false, true, true)->fval = 9.0f;
		const wchar_t *source_name = cached ? L"resourcecachedmissing" : L"resourcefreshmissing";
		customResources[source_name].substantiated = cached;
		CommandListScope scope;
		CommandList commands;
		commands.scope = &scope;
		const std::wstring destination = std::wstring(pool_name) + L"[$" + pool_name + L"[0]]";
		const std::wstring source = std::wstring(L"ref unless_null ") + source_name;
		parse_copy(commands, destination.c_str(), source.c_str());
		G->frame_no = 14;
		for (const auto &command : commands.commands)
			command->run(&fixture.state);
		G->frame_no = 17;
		pool.read_refreshes_expiration = false;
		require(pool.GetVariable(0, false, true, false)->fval == 3.0f,
		        "skipped copy refreshed expiration through its nested destination index");
		G->frame_no = 20;
		pool.read_refreshes_expiration = true;
		CommandListVariable *retained = pool.GetVariable(0, false, true, true);
		retained->fval = 11.0f;
		G->frame_no = 24;
		CommandListExpression normal_read;
		const std::wstring variable = std::wstring(L"$") + pool_name + L"[0]";
		require(normal_read.parse(&variable, &config_namespace, &scope), "normal pool read was rejected");
		require(normal_read.evaluate(&fixture.state) == 11.0f, "normal pool read changed the variable value");
		G->frame_no = 27;
		pool.read_refreshes_expiration = false;
		require(pool.GetVariable(0, false, true, false)->fval == 11.0f,
		        "metadata evaluation disabled expiration refresh for a subsequent normal read");
		G->frame_no = 31;
		auto preview = pool.CreateMetadataPreview();
		require(preview->GetVariable(0, false, false, false)->fval == 3.0f && retained->fval == 11.0f,
		        "metadata read did not expose the expired default without resetting stored data");
	}
	fixture.require_no_errors();
}

void missing_resource_metadata_has_no_bind_flags()
{
	CommandListState state;
	ResourceCopyTarget target;
	target.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
	D3D11_RESOURCE_MISC_FLAG misc = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	require(target.BindFlags(&state, &misc) == 0 && misc == 0,
	        "missing resource metadata did not return empty binding requirements");
}

void accepts_pool_identifier_grammar()
{
	ConfigFixture resources;
	make_pool(L"poolhyphen-name", 2.5f);
	make_pool(L"pool\\test.ini\\\u03b4-name", 4.0f);
	make_pool(L"pool\u03b4", 6.0f);
	command_list_globals.emplace(L"$pool_test", CommandListVariable(L"$pool_test", 8.0f, VariableFlags::NONE));
	CommandListState state;
	CommandListScope scope;
	const struct
	{
		const wchar_t *text;
		float expected;
	} cases[] = {{L"$poolhyphen-name[0]", 2.5f},
	             {L"$pool\\test.ini\\\u03b4-name[0]", 4.0f},
	             {L"$pool\u03b4[0]", 6.0f},
	             {L"$pool_test - 1", 7.0f}};
	for (const auto &item : cases)
	{
		CommandListExpression expression;
		const std::wstring input(item.text);
		require(expression.parse(&input, &config_namespace, &scope), "pool identifier was rejected");
		require(expression.evaluate(&state) == item.expected, "pool identifier resolved the wrong value");
	}
}

void read_store(StoreCommand &command, CommandListState &state, const wchar_t *index)
{
	command.offset_expression = std::make_unique<CommandListExpression>();
	const std::wstring text(index);
	require(command.offset_expression->parse(&text, &config_namespace, nullptr), "store index was rejected");
	command.run(&state);
}

void stores_typed_and_structured_values()
{
	D3DFixture fixture;
	for (bool structured : {false, true})
	{
		CustomResource source;
		const float values[] = {1.25f, 2.5f, 3.75f, 5.0f, 6.25f, 7.5f, 8.75f, 10.0f};
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = sizeof(values);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.MiscFlags = structured ? D3D11_RESOURCE_MISC_BUFFER_STRUCTURED : 0;
		desc.StructureByteStride = structured ? 16 : 0;
		D3D11_SUBRESOURCE_DATA data{};
		data.pSysMem = values;
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		require(SUCCEEDED(fixture.device->CreateBuffer(&desc, &data, &buffer)), "store source creation failed");
		source.resource = buffer.Detach();
		source.device = fixture.device.Get();
		source.is_null = false;
		CommandListVariable value(L"$result", -99.0f, VariableFlags::NONE);
		StoreCommand command;
		command.src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
		command.src.SetCustomResource(&source);
		command.var = &value;
		read_store(command, fixture.state, L"0");
		require(value.fval == 1.25f, "store did not read the first float");
		read_store(command, fixture.state, L"3");
		require(value.fval == 5.0f, "store did not read the final float in a structure");
		read_store(command, fixture.state, L"4");
		require(value.fval == 6.25f, "store did not cross to the next structure");
		read_store(command, fixture.state, L"7");
		require(value.fval == 10.0f, "store did not read the exact-fit final float");
		for (const wchar_t *invalid : {L"-1", L"8", L"4294967296", L"1e30", L"nan", L"inf"})
		{
			value.fval = -99.0f;
			read_store(command, fixture.state, invalid);
			require(value.fval == -99.0f, "invalid store index overwrote the previous value");
		}
	}
	fixture.require_no_errors();
}

class ViewLifetime final : public IUnknown
{
	LONG references = 1;
	std::shared_ptr<std::atomic_bool> destroyed;

  public:
	explicit ViewLifetime(std::shared_ptr<std::atomic_bool> destroyed) : destroyed(std::move(destroyed)) {}
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **result) override
	{
		if (!result)
			return E_POINTER;
		*result = nullptr;
		if (iid != __uuidof(IUnknown))
			return E_NOINTERFACE;
		*result = static_cast<IUnknown *>(this);
		AddRef();
		return S_OK;
	}
	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return static_cast<ULONG>(InterlockedIncrement(&references));
	}
	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG remaining = static_cast<ULONG>(InterlockedDecrement(&references));
		if (!remaining)
		{
			destroyed->store(true, std::memory_order_release);
			delete this;
		}
		return remaining;
	}
};

void releases_store_views_on_success_and_failure()
{
	D3DFixture fixture;
	for (bool buffer_source : {true, false})
	{
		CustomResource source;
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
		if (buffer_source)
		{
			const float values[] = {7.0f, 8.0f, 9.0f, 10.0f};
			D3D11_BUFFER_DESC desc{};
			desc.ByteWidth = sizeof(values);
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA initial{};
			initial.pSysMem = values;
			Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
			require(SUCCEEDED(fixture.device->CreateBuffer(&desc, &initial, &buffer)), "viewed buffer creation failed");
			require(SUCCEEDED(buffer.As(&resource)), "buffer resource interface unavailable");
			D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
			view_desc.Format = DXGI_FORMAT_R32_FLOAT;
			view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			view_desc.Buffer.NumElements = 4;
			require(SUCCEEDED(fixture.device->CreateShaderResourceView(resource.Get(), &view_desc, &view)),
			        "buffer view creation failed");
		}
		else
		{
			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = desc.Height = desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
			desc.Format = DXGI_FORMAT_R32_FLOAT;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
			require(SUCCEEDED(fixture.device->CreateTexture2D(&desc, nullptr, &texture)),
			        "texture source creation failed");
			require(SUCCEEDED(texture.As(&resource)), "texture resource interface unavailable");
			require(SUCCEEDED(fixture.device->CreateShaderResourceView(resource.Get(), nullptr, &view)),
			        "texture view creation failed");
		}
		auto destroyed = std::make_shared<std::atomic_bool>(false);
		auto *lifetime = new ViewLifetime(destroyed);
		const GUID key = {0x83e75508, 0xe509, 0x4ca9, {0x81, 0xbe, 0xec, 0x6d, 0x11, 0xa4, 0x4f, 0x18}};
		require(SUCCEEDED(view->SetPrivateDataInterface(key, lifetime)), "could not track view lifetime");
		lifetime->Release();
		source.resource = resource.Detach();
		source.device = fixture.device.Get();
		source.view = view.Detach();
		source.is_null = false;
		auto *bound = static_cast<ID3D11ShaderResourceView *>(source.view);
		fixture.context->PSSetShaderResources(0, 1, &bound);
		CommandListVariable value(L"$result", 42.0f, VariableFlags::NONE);
		StoreCommand command;
		command.src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
		command.src.SetCustomResource(&source);
		command.var = &value;
		for (int iteration = 0; iteration < 4; ++iteration)
		{
			if (iteration == 2)
			{
				command.src.type = ResourceCopyTargetType::SHADER_RESOURCE;
				command.src.shader_type = L'p';
			}
			value.fval = 42.0f;
			read_store(command, fixture.state, L"0");
			require(value.fval == (buffer_source ? 7.0f : 42.0f),
			        "store returned the wrong value or changed a failed result");
		}
		source.view->Release();
		source.view = nullptr;
		fixture.context->ClearState();
		fixture.context->Flush();
		require(destroyed->load(std::memory_order_acquire), "store retained an owned source view reference");
	}
	fixture.require_no_errors();
}

std::vector<float> read_buffer(D3DFixture &fixture, ID3D11Resource *resource)
{
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	require(SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&buffer))), "result is not a buffer");
	D3D11_BUFFER_DESC desc{};
	buffer->GetDesc(&desc);
	const UINT byte_width = desc.ByteWidth;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = desc.MiscFlags = desc.StructureByteStride = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	Microsoft::WRL::ComPtr<ID3D11Buffer> staging;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, nullptr, &staging)), "readback buffer creation failed");
	fixture.context->CopyResource(staging.Get(), resource);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	require(SUCCEEDED(fixture.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "result readback failed");
	std::vector<float> result(byte_width / sizeof(float));
	memcpy(result.data(), mapped.pData, byte_width);
	fixture.context->Unmap(staging.Get(), 0);
	return result;
}

void copies_region_byte_counts_and_preserves_invalid_destinations()
{
	D3DFixture fixture;
	ConfigFixture resources;
	const float values[] = {1.25f, 2.5f, 3.75f, 5.0f, 6.25f, 7.5f, 8.75f, 10.0f};
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(values);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = values;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, &initial, &buffer)), "region source creation failed");
	auto &source = customResources[L"resourcesource"];
	source.resource = buffer.Detach();
	source.device = fixture.device.Get();
	source.is_null = false;
	source.stride = 4;
	source.format = DXGI_FORMAT_R32_FLOAT;
	auto &destination = customResources[L"resourcedestination"];
	destination.bind_flags = D3D11_BIND_SHADER_RESOURCE;
	CommandList commands;
	parse_copy(commands, L"resourcedestination", L"copy resourcesource->region(8, 12)");
	for (const auto &command : commands.commands)
		command->run(&fixture.state);
	require(destination.resource != nullptr, "region copy did not create a destination");
	require(read_buffer(fixture, destination.resource) == std::vector<float>({3.75f, 5.0f, 6.25f}),
	        "region size was not a byte count");
	CommandList cb_copy;
	parse_copy(cb_copy, L"vs-cb2", L"copy resourcesource->region(8, 12)");
	for (const auto &command : cb_copy.commands)
		command->run(&fixture.state);
	Microsoft::WRL::ComPtr<ID3D11Buffer> copied_cb;
	fixture.context->VSGetConstantBuffers(2, 1, &copied_cb);
	require(copied_cb != nullptr, "copied region incorrectly used a ranged CB binding");
	const auto cb_values = read_buffer(fixture, copied_cb.Get());
	require(cb_values.size() == 4 && cb_values[0] == 3.75f && cb_values[1] == 5.0f && cb_values[2] == 6.25f,
	        "copied CB region did not preserve its requested bytes");
	auto *previous = destination.resource;
	for (const wchar_t *invalid : {L"resourcesource->region(-1, 4)", L"resourcesource->region(32, 1)",
	                               L"resourcesource->region(31, 2)", L"resourcesource->region(4294967295, 8)",
	                               L"resourcesource->region(1e30, 4)", L"resourcesource->region(0, 0)"})
	{
		CommandList invalid_command;
		parse_copy(invalid_command, L"resourcedestination", (L"copy " + std::wstring(invalid)).c_str());
		for (const auto &command : invalid_command.commands)
			command->run(&fixture.state);
		require(destination.resource == previous, "invalid region replaced the destination resource");
		require(read_buffer(fixture, destination.resource) == std::vector<float>({3.75f, 5.0f, 6.25f}),
		        "invalid region changed destination data");
	}
	fixture.require_no_errors();
}

void vertex_regions_use_byte_counts_instead_of_binding_stride()
{
	D3DFixture fixture;
	ConfigFixture resources;
	const float values[] = {3, 5, 7, 11, 13, 17};
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(values);
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = values;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, &initial, &buffer)), "vertex source creation failed");
	ID3D11Buffer *binding = buffer.Get();
	const UINT stride = 12, offset = 0;
	fixture.context->IASetVertexBuffers(0, 1, &binding, &stride, &offset);
	CommandList commands;
	parse_copy(commands, L"vb1", L"copy vb0->region(0,4)");
	for (const auto &command : commands.commands)
		command->run(&fixture.state);
	Microsoft::WRL::ComPtr<ID3D11Buffer> copied;
	UINT copied_stride = 0, copied_offset = 0;
	fixture.context->IAGetVertexBuffers(1, 1, &copied, &copied_stride, &copied_offset);
	require(copied != nullptr, "vertex region did not bind a destination");
	require(read_buffer(fixture, copied.Get()) == std::vector<float>({3}),
	        "vertex binding stride truncated the requested four-byte copy");
	fixture.require_no_errors();
}

void region_views_follow_changing_ranges()
{
	D3DFixture fixture;
	ConfigFixture resources;
	const float values[] = {1, 2, 3, 4, 5, 6, 7, 8};
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(values);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = values;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, &initial, &buffer)), "view source creation failed");
	D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
	view_desc.Format = DXGI_FORMAT_R32_FLOAT;
	view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	view_desc.Buffer.NumElements = 8;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> source_view;
	require(SUCCEEDED(fixture.device->CreateShaderResourceView(buffer.Get(), &view_desc, &source_view)),
	        "full source view creation failed");
	auto *binding = source_view.Get();
	fixture.context->PSSetShaderResources(0, 1, &binding);
	auto inserted =
	    command_list_globals.emplace(L"$offset", CommandListVariable(L"$offset", 8.0f, VariableFlags::NONE));
	CommandList commands;
	parse_copy(commands, L"ps-t1", L"ref ps-t0->region($offset, 8)");
	for (float offset : {8.0f, 16.0f})
	{
		inserted.first->second.fval = offset;
		for (const auto &command : commands.commands)
			command->run(&fixture.state);
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> result;
		fixture.context->PSGetShaderResources(1, 1, &result);
		require(result != nullptr, "region reference did not bind a view");
		result->GetDesc(&view_desc);
		require(view_desc.Buffer.FirstElement == (offset == 8.0f ? 2U : 4U),
		        "region reference reused a stale/full view");
		require(view_desc.Buffer.NumElements == 2, "region reference exposed bytes outside its requested range");
	}
	fixture.require_no_errors();
}

void copies_cb_regions_beyond_the_bound_window()
{
	D3DFixture fixture;
	ConfigFixture resources;
	float values[256]{};
	values[128] = 11;
	values[129] = 12;
	values[255] = 77;
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(values);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = values;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, &initial, &buffer)), "CB backing buffer creation failed");
	auto *binding = buffer.Get();
	const UINT first = 16, count = 16;
	fixture.context->VSSetConstantBuffers1(0, 1, &binding, &first, &count);
	auto &destination = customResources[L"resourcedestination"];
	destination.bind_flags = D3D11_BIND_VERTEX_BUFFER;
	CommandList commands;
	parse_copy(commands, L"resourcedestination", L"copy vs-cb0->region(512, 512)");
	for (const auto &command : commands.commands)
		command->run(&fixture.state);
	require(destination.resource != nullptr, "CB region copy did not create a destination");
	const auto result = read_buffer(fixture, destination.resource);
	require(result.size() == 128, "explicit CB region was clamped to the visible window");
	require(result[0] == 11.0f && result[1] == 12.0f && result[127] == 77.0f,
	        "CB region copied the wrong backing bytes");
	CommandList reference;
	parse_copy(reference, L"cs-cb1", L"ref vs-cb0->region(256, 256)");
	for (const auto &command : reference.commands)
		command->run(&fixture.state);
	UINT actual_first = 0, actual_count = 0;
	Microsoft::WRL::ComPtr<ID3D11Buffer> referenced;
	fixture.context->CSGetConstantBuffers1(1, 1, &referenced, &actual_first, &actual_count);
	require(referenced == buffer && actual_first == 16 && actual_count == 16,
	        "valid CB reference did not bind the requested aligned window");
	CommandList unaligned;
	parse_copy(unaligned, L"cs-cb1", L"ref vs-cb0->region(16, 16)");
	for (const auto &command : unaligned.commands)
		command->run(&fixture.state);
	referenced.Reset();
	fixture.context->CSGetConstantBuffers1(1, 1, &referenced, &actual_first, &actual_count);
	require(referenced == buffer && actual_first == 16 && actual_count == 16,
	        "unsupported CB reference alignment changed the previous binding");
	fixture.require_no_errors();
}

void preserves_pending_save()
{
	const std::filesystem::path root = std::filesystem::current_path() / L".quality" / L"settings-test";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	const auto blocker = root / L"not-a-directory";
	std::ofstream(blocker) << "block";
	CommandListVariable variable(L"$test", 7.5f, VariableFlags::PERSIST);
	persistent_variables.push_back(&variable);
	G->user_config = (blocker / L"settings.ini").wstring();
	G->user_config_dirty = 3;
	G->gTime = 12.0f;
	G->gDefaultLocale = "C";
	setlocale(LC_CTYPE, "C");
	SavePersistentSettings();
	const int dirty_after_failure = G->user_config_dirty;
	const std::string locale_after_failure = setlocale(LC_CTYPE, nullptr);
	G->user_config = (root / L"settings.ini").wstring();
	SavePersistentSettings();
	std::ifstream saved(root / L"settings.ini");
	const std::string contents((std::istreambuf_iterator<char>(saved)), std::istreambuf_iterator<char>());
	saved.close();
	persistent_variables.clear();
	std::filesystem::remove_all(root);
	require(dirty_after_failure == 3, "failed save discarded pending settings");
	require(locale_after_failure == "C", "failed save changed the caller's locale");
	require(contents.find("$test = 7.5\n") != std::string::npos, "retry did not save the pending value");
	require(G->user_config_dirty == 0, "successful save left settings dirty");
	require(G->gSettingsSaveTime == 12.0f, "save scheduling changed");
}

std::string file_contents(const std::filesystem::path &path)
{
	std::ifstream input(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void failed_replacement_preserves_complete_file()
{
	const auto root = std::filesystem::current_path() / L".quality" / L"atomic-settings-test";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	const auto path = root / L"settings.ini";
	std::ofstream(path, std::ios::binary) << "previous complete settings\n";
	struct FileLock
	{
		HANDLE handle = INVALID_HANDLE_VALUE;
		~FileLock()
		{
			if (handle != INVALID_HANDLE_VALUE)
				CloseHandle(handle);
		}
	} lock;
	lock.handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
	                          FILE_ATTRIBUTE_NORMAL, nullptr);
	require(lock.handle != INVALID_HANDLE_VALUE, "could not prevent destination replacement");
	CommandListVariable variable(L"$atomic", 1.25f, VariableFlags::PERSIST);
	persistent_variables.push_back(&variable);
	G->user_config = path.wstring();
	G->user_config_dirty = 3;
	setlocale(LC_CTYPE, "C");
	const bool failed_wipe = WipeUserConfig();
	const bool failed_save = SavePersistentSettings();
	const auto old_contents = file_contents(path);
	const int dirty_after_failure = G->user_config_dirty;
	const std::string locale_after_failure = setlocale(LC_CTYPE, nullptr);
	CloseHandle(lock.handle);
	lock.handle = INVALID_HANDLE_VALUE;
	const bool retry = SavePersistentSettings();
	const auto saved = file_contents(path);
	persistent_variables.clear();
	std::filesystem::remove_all(root);
	require(!failed_save, "replacement unexpectedly succeeded while deletion was denied");
	require(!failed_wipe, "wipe unexpectedly succeeded while deletion was denied");
	require(old_contents == "previous complete settings\n", "failed replacement damaged the last complete file");
	require(dirty_after_failure == 3, "failed replacement discarded pending settings");
	require(locale_after_failure == "C", "failed replacement changed the caller locale");
	require(retry && saved.find("$atomic = 1.25") != std::string::npos, "replacement retry did not save the value");
}

struct SettingsConfigFixture
{
	std::filesystem::path config;
	std::filesystem::path user;
	std::vector<std::filesystem::path> includes;

	SettingsConfigFixture()
	{
		wchar_t executable[MAX_PATH];
		require(GetModuleFileNameW(nullptr, executable, ARRAYSIZE(executable)) != 0,
		        "could not locate test executable");
		const auto directory = std::filesystem::path(executable).parent_path();
		config = directory / L"d3dx.ini";
		user = directory / L"runtime-user.ini";
		require(!std::filesystem::exists(config) && !std::filesystem::exists(user),
		        "configuration checks require an isolated test directory");
		write_config(true);
	}

	void write_config(bool clear_unknown)
	{
		std::ofstream(config) << "[Logging]\nenabled=0\ncalls=0\ndebug=0\nshow_warnings=0\n"
		                         "[System]\nclear_unknown_settings="
		                      << (clear_unknown ? "1" : "0")
		                      << "\n[Include]\nuser_config=runtime-user.ini\n[Constants]\n"
		                         "global persist $known = 1\nglobal $temporary = 5\n";
	}

	~SettingsConfigFixture()
	{
		std::error_code error;
		std::filesystem::remove(config, error);
		std::filesystem::remove(user, error);
		for (const auto &path : includes)
			std::filesystem::remove(path, error);
	}
};

void deferred_initialization_preserves_unloaded_settings()
{
	D3DFixture fixture;
	ConfigFixture resources;
	SettingsConfigFixture files;
	const auto included = files.config.parent_path() / L"runtime-deferred.ini";
	files.includes.push_back(included);
	std::ofstream(included) << "namespace = deferred\n[Constants]\nglobal persist $included = 2\n";
	std::ofstream(files.user) << "[Constants]\n$boot = 7\n$\\deferred\\included = 13\n";
	std::ofstream(files.config) << "[Logging]\nenabled=0\nshow_warnings=0\n"
	                               "[System]\nskip_early_includes_load=1\n"
	                               "[Include]\nuser_config=runtime-user.ini\ninclude=runtime-deferred.ini\n"
	                               "[Constants]\nglobal persist $boot = 0\npost $boot = 1\n";
	G->user_config.clear();
	G->gConfigInitialized = false;
	LoadConfigFile();
	for (const auto &command : G->constants_command_list.commands)
		command->run(&fixture.state);
	G->user_config_dirty &= ~1;
	for (const auto &command : G->post_constants_command_list.commands)
		command->run(&fixture.state);
	require(G->user_config_dirty != 0, "post-Constants did not leave a pending startup change");
	require(SavePersistentSettings(), "unresolved settings destination blocked deferred initialization");
	G->gConfigInitialized = true;
	LoadConfigFile();
	for (const auto &command : G->constants_command_list.commands)
		command->run(&fixture.state);
	require(command_list_globals.at(L"$boot").fval == 1.0f, "deferred initialization lost the pending startup value");
	require(command_list_globals.at(L"$\\deferred\\included").fval == 13.0f,
	        "startup save overwrote a persistent value whose include had not loaded yet");
}

VariableAssignment *assignment_during_replace = nullptr;
CommandListState *state_during_replace = nullptr;
decltype(&MoveFileExW) real_move_file = nullptr;

BOOL WINAPI replace_with_pending_assignment(LPCWSTR existing, LPCWSTR replacement, DWORD flags)
{
	assignment_during_replace->run(state_during_replace);
	return real_move_file(existing, replacement, flags);
}

void saves_acknowledge_only_the_captured_settings()
{
	D3DFixture fixture;
	ConfigFixture resources;
	SettingsConfigFixture files;
	CommandListVariable variable(L"$test", 7.0f, VariableFlags::PERSIST);
	persistent_variables.push_back(&variable);
	G->user_config = files.user.wstring();
	G->user_config_dirty = 1;
	VariableAssignment assignment;
	assignment.var = &variable;
	const std::wstring value = L"11";
	require(assignment.expression.parse(&value, nullptr, nullptr), "save boundary assignment rejected");
	assignment_during_replace = &assignment;
	state_during_replace = &fixture.state;
	auto address = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "MoveFileExW");
	static_assert(sizeof(address) == sizeof(real_move_file));
	memcpy(&real_move_file, &address, sizeof(address));
	require(real_move_file != nullptr, "filesystem replacement boundary unavailable");
	{
		ImportBoundary boundary(GetModuleHandleW(nullptr), "MoveFileExW", &replace_with_pending_assignment);
		require(SavePersistentSettings(), "first save failed");
	}
	require(file_contents(files.user).find("$test = 7\r\n") != std::string::npos, "save did not write its snapshot");
	require(variable.fval == 11.0f && G->user_config_dirty != 0, "completed save erased a newer pending change");
	require(SavePersistentSettings(), "save of the newer change failed");
	require(file_contents(files.user).find("$test = 11\r\n") != std::string::npos && G->user_config_dirty == 0,
	        "next save did not commit the newer pending value");
	assignment_during_replace = nullptr;
	state_during_replace = nullptr;
}

void reload_defers_until_deferred_command_batches_finish()
{
	D3DFixture fixture;
	ConfigFixture resources;
	SettingsConfigFixture files;
	std::ofstream(files.user) << "[Constants]\n$known = 7\n";
	LoadConfigFile();
	for (const auto &command : G->constants_command_list.commands)
		command->run(&fixture.state);
	HackerDevice device(fixture.device.Get(), fixture.context.Get());
	device.SetHackerContext(fixture.hacker_context.get());
	fixture.hacker_context->SetHackerDevice(&device, false);
	Microsoft::WRL::ComPtr<ID3D11DeviceContext1> deferred;
	require(SUCCEEDED(fixture.device->CreateDeferredContext1(0, &deferred)), "deferred context creation failed");
	HackerContext deferred_context(fixture.device.Get(), deferred.Get());
	CommandListState state;
	state.mHackerContext = &deferred_context;
	for (bool wipe : {false, true})
	{
		std::exception_ptr reload_error;
		{
			// This is the same batch object the production Dispatch entry point
			// creates before borrowing the ShaderOverride and its command lists.
			DispatchContext batch(1, 1, 1);
			VariableAssignment assignment;
			assignment.var = &command_list_globals.at(L"$known");
			const std::wstring value = wipe ? L"13" : L"11";
			require(assignment.expression.parse(&value, nullptr, nullptr), "deferred assignment rejected");
			assignment.run(&state);
			command_list_globals.at(L"$temporary").SetValue(9.0f);
			G->gWipeUserConfig = wipe;
			std::thread reload(
			    [&]()
			    {
				    try
				    {
					    ReloadConfig(&device);
				    }
				    catch (...)
				    {
					    reload_error = std::current_exception();
				    }
			    });
			reload.join();
			if (reload_error)
				std::rethrow_exception(reload_error);
			require(G->gReloadConfigPending && command_list_globals.at(L"$temporary").fval == 9.0f,
			        "reload replaced configuration owned by an active deferred batch");
			require(command_list_globals.at(L"$known").fval == (wipe ? 13.0f : 11.0f),
			        "deferred assignment changed during a deferred reload");
			require(file_contents(files.user).find(wipe ? "$known = 11" : "$known = 7") != std::string::npos,
			        "deferred reload changed the saved file before the batch finished");
		}
		ReloadConfig(&device);
		require(!G->gReloadConfigPending && command_list_globals.at(L"$temporary").fval == 5.0f,
		        "reload did not apply after the deferred batch finished");
		require(command_list_globals.at(L"$known").fval == (wipe ? 1.0f : 11.0f),
		        "reload lost its pending value or failed to apply the explicit wipe");
		if (wipe)
			require(!std::filesystem::exists(files.user), "preflight recreated settings after an explicit wipe");
	}
}

HANDLE save_ready = nullptr;
HANDLE finish_save = nullptr;
std::atomic<DWORD> saving_thread{0};
std::atomic<PSRWLOCK> save_file_lock{nullptr};
decltype(&AcquireSRWLockExclusive) real_acquire_srw = nullptr;

void WINAPI acquire_after_releasing_file_operation(PSRWLOCK lock)
{
	if (GetCurrentThreadId() == saving_thread.load())
	{
		PSRWLOCK empty = nullptr;
		save_file_lock.compare_exchange_strong(empty, lock);
	}
	else if (lock == save_file_lock.load())
		SetEvent(finish_save);
	real_acquire_srw(lock);
}

BOOL WINAPI hold_first_file_replacement(LPCWSTR existing, LPCWSTR replacement, DWORD flags)
{
	if (GetCurrentThreadId() == saving_thread.load())
	{
		SetEvent(save_ready);
		if (!save_file_lock.load())
			return FALSE;
		WaitForSingleObject(finish_save, INFINITE);
	}
	return real_move_file(existing, replacement, flags);
}

void reload_waits_for_an_inflight_autosave()
{
	D3DFixture fixture;
	ConfigFixture resources;
	SettingsConfigFixture files;
	std::ofstream(files.user) << "[Constants]\n$known = 3\n";
	LoadConfigFile();
	for (const auto &command : G->constants_command_list.commands)
		command->run(&fixture.state);
	command_list_globals.at(L"$known").SetValue(7.0f);
	HackerDevice device(fixture.device.Get(), fixture.context.Get());
	device.SetHackerContext(fixture.hacker_context.get());
	fixture.hacker_context->SetHackerDevice(&device, false);
	struct CloseEvents
	{
		CloseEvents() = default;
		CloseEvents(const CloseEvents &) = delete;
		CloseEvents &operator=(const CloseEvents &) = delete;
		CloseEvents(CloseEvents &&) = delete;
		CloseEvents &operator=(CloseEvents &&) = delete;
		~CloseEvents()
		{
			if (save_ready)
				CloseHandle(save_ready);
			if (finish_save)
				CloseHandle(finish_save);
			save_ready = finish_save = nullptr;
			save_file_lock = nullptr;
			saving_thread = 0;
		}
	} close_events;
	save_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	finish_save = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	require(save_ready && finish_save, "save boundary events unavailable");
	auto address = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "MoveFileExW");
	memcpy(&real_move_file, &address, sizeof(address));
	address = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "AcquireSRWLockExclusive");
	static_assert(sizeof(address) == sizeof(real_acquire_srw));
	memcpy(&real_acquire_srw, &address, sizeof(address));
	require(real_move_file && real_acquire_srw, "save boundary imports unavailable");
	ImportBoundary file_boundary(GetModuleHandleW(nullptr), "MoveFileExW", &hold_first_file_replacement);
	ImportBoundary lock_boundary(GetModuleHandleW(nullptr), "AcquireSRWLockExclusive",
	                             &acquire_after_releasing_file_operation);
	bool saved = false;
	std::exception_ptr save_error;
	std::thread worker(
	    [&]()
	    {
		    saving_thread = GetCurrentThreadId();
		    try
		    {
			    saved = SavePersistentSettings();
		    }
		    catch (...)
		    {
			    save_error = std::current_exception();
			    SetEvent(save_ready);
		    }
		    SetEvent(save_ready);
	    });
	WaitForSingleObject(save_ready, INFINITE);
	try
	{
		ReloadConfig(&device);
	}
	catch (...)
	{
		SetEvent(finish_save);
		worker.join();
		throw;
	}
	// A bypassed preflight reaches here while the old file is still in place;
	// a correct preflight releases the filesystem boundary when it waits on
	// the in-flight operation's real OS lock. No sleeps or clock thresholds.
	SetEvent(finish_save);
	worker.join();
	if (save_error)
		std::rethrow_exception(save_error);
	require(saved && save_file_lock.load(), "in-flight save/lock boundary failed");
	require(command_list_globals.at(L"$known").fval == 7.0f, "reload read the file before the autosave committed");
	require(file_contents(files.user).find("$known = 7") != std::string::npos,
	        "autosave did not preserve its snapshot");
}

HackerDevice *analysis_reload_device = nullptr;
VariableAssignment *analysis_assignment = nullptr;
CommandListState *analysis_assignment_state = nullptr;
std::exception_ptr analysis_reload_error;
bool analysis_probe_enabled = false;
bool analysis_probe_seen = false;
decltype(&EnterCriticalSection) real_analysis_enter = nullptr;

void WINAPI reject_contended_analysis_lock(LPCRITICAL_SECTION section)
{
	if (section == &G->mCriticalSection)
	{
		// Reject the OS wait instead of hanging on the writer/global-lock
		// inversion this fixture protects. Other OS locks behave normally.
		if (!TryEnterCriticalSection(section))
			throw std::runtime_error("reload would wait behind an active frame-analysis dump");
		return;
	}
	real_analysis_enter(section);
}

HWND WINAPI poll_analysis_input()
{
	if (analysis_probe_enabled && !analysis_probe_seen)
	{
		analysis_probe_seen = true;
		try
		{
			analysis_assignment->run(analysis_assignment_state);
			std::thread reload(
			    []()
			    {
				    try
				    {
					    ReloadConfig(analysis_reload_device);
				    }
				    catch (...)
				    {
					    analysis_reload_error = std::current_exception();
				    }
			    });
			reload.join();
		}
		catch (...)
		{
			analysis_reload_error = std::current_exception();
		}
	}
	// Keep the host's actual keyboard/controllers outside this fixture.
	return nullptr;
}

void frame_analysis_input_keeps_configuration_ownership()
{
	for (int route = 0; route < 4; ++route)
	{
		D3DFixture fixture;
		ConfigFixture resources;
		SettingsConfigFixture files;
		std::ofstream(files.user) << "[Constants]\n$known = 7\n";
		LoadConfigFile();
		for (const auto &command : G->constants_command_list.commands)
			command->run(&fixture.state);
		const auto output = files.config.parent_path() / L"runtime-analysis-locks";
		std::filesystem::create_directories(output);
		struct RestoreAnalysis
		{
			std::filesystem::path directory;
			std::wstring previous_path = G->ANALYSIS_PATH;
			FrameAnalysisOptions previous_current = G->cur_analyse_options;
			FrameAnalysisOptions previous_default = G->def_analyse_options;
			bool previous_foreground = G->check_foreground_window;
			bool previous_analyzing = G->analyse_frame;
			UINT previous_hunting = G->hunting;
			ShaderMap previous_shaders = G->mShaders;
			std::set<UINT64> previous_visited = G->mVisitedComputeShaders;
			explicit RestoreAnalysis(const std::filesystem::path &directory) : directory(directory) {}
			RestoreAnalysis(const RestoreAnalysis &) = delete;
			RestoreAnalysis &operator=(const RestoreAnalysis &) = delete;
			RestoreAnalysis(RestoreAnalysis &&) = delete;
			RestoreAnalysis &operator=(RestoreAnalysis &&) = delete;
			~RestoreAnalysis()
			{
				G->analyse_frame = previous_analyzing;
				G->hunting = previous_hunting;
				G->mShaders = std::move(previous_shaders);
				G->mVisitedComputeShaders = std::move(previous_visited);
				G->cur_analyse_options = previous_current;
				G->def_analyse_options = previous_default;
				G->check_foreground_window = previous_foreground;
				wcscpy_s(G->ANALYSIS_PATH, previous_path.c_str());
				analysis_probe_enabled = analysis_probe_seen = false;
				analysis_reload_error = nullptr;
				analysis_reload_device = nullptr;
				analysis_assignment = nullptr;
				analysis_assignment_state = nullptr;
				std::error_code error;
				std::filesystem::remove_all(directory, error);
			}
		} cleanup(output);
		require(wcscpy_s(G->ANALYSIS_PATH, output.wstring().c_str()) == 0, "analysis output path is too long");
		HackerDevice device(fixture.device.Get(), fixture.context.Get());
		device.SetHackerContext(fixture.hacker_context.get());
		fixture.hacker_context->SetHackerDevice(&device, false);
		Microsoft::WRL::ComPtr<ID3D11DeviceContext1> deferred;
		require(SUCCEEDED(fixture.device->CreateDeferredContext1(0, &deferred)), "analysis deferred context failed");
		FrameAnalysisContext analysis(fixture.device.Get(), deferred.Get());
		FrameAnalysisContext playback(fixture.device.Get(), fixture.context.Get());
		analysis.SetHackerDevice(&device, false);
		playback.SetHackerDevice(&device, false);
		const char program[] = "[numthreads(1,1,1)] void main(uint3 id:SV_DispatchThreadID) {}";
		Microsoft::WRL::ComPtr<ID3DBlob> code, errors;
		require(SUCCEEDED(D3DCompile(program, sizeof(program) - 1, "analysis-lock", nullptr, nullptr, "main", "cs_5_0",
		                             0, 0, &code, &errors)),
		        "analysis compute shader compilation failed");
		Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader;
		require(SUCCEEDED(fixture.device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr,
		                                                      &shader)),
		        "analysis compute shader creation failed");
		const float values[] = {1, 2, 3, 4};
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = sizeof(values);
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		D3D11_SUBRESOURCE_DATA initial{};
		initial.pSysMem = values;
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		require(SUCCEEDED(fixture.device->CreateBuffer(&desc, &initial, &buffer)), "analysis constant buffer failed");
		// The raw D3D fixture needs the identity normally registered by HackerDevice.
		G->mShaders[shader.Get()] = 0x1234;
		G->hunting = HUNTING_MODE_ENABLED;
		analysis.CSSetShader(shader.Get(), nullptr, 0);
		auto *binding = buffer.Get();
		analysis.CSSetConstantBuffers(0, 1, &binding);
		G->check_foreground_window = true;
		G->analyse_frame = true;
		auto options = FrameAnalysisOptions::HOLD;
		options |= FrameAnalysisOptions::DUMP_CB;
		options |= FrameAnalysisOptions::FMT_BUF_BIN;
		options |= FrameAnalysisOptions::MONO;
		options |= route == 3 ? FrameAnalysisOptions::DEFRD_CTX_DELAY : FrameAnalysisOptions::DEFRD_CTX_IMM;
		if (route == 1)
			options |= FrameAnalysisOptions::DUMP_ON_UPDATE;
		G->cur_analyse_options = G->def_analyse_options = options;
		VariableAssignment assignment;
		assignment.var = &command_list_globals.at(L"$known");
		const std::wstring value = L"11";
		require(assignment.expression.parse(&value, nullptr, nullptr), "analysis assignment rejected");
		CommandListState state;
		state.mHackerContext = &analysis;
		analysis_assignment = &assignment;
		analysis_assignment_state = &state;
		analysis_reload_device = &device;
		auto address = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "EnterCriticalSection");
		static_assert(sizeof(address) == sizeof(real_analysis_enter));
		memcpy(&real_analysis_enter, &address, sizeof(address));
		require(real_analysis_enter != nullptr, "analysis lock boundary unavailable");
		{
			ImportBoundary foreground(GetModuleHandleW(nullptr), "GetForegroundWindow", &poll_analysis_input);
			ImportBoundary critical(GetModuleHandleW(nullptr), "EnterCriticalSection", &reject_contended_analysis_lock);
			analysis_probe_enabled = route != 3;
			if (route == 0)
				analysis.Dispatch(1, 1, 1);
			else if (route == 1)
				analysis.UpdateSubresource(buffer.Get(), 0, nullptr, values, 0, 0);
			else if (route == 2)
				analysis.FrameAnalysisDump(buffer.Get(), options, L"lock-test", DXGI_FORMAT_UNKNOWN, 0, 0);
			else
			{
				analysis.Dispatch(1, 1, 1);
				Microsoft::WRL::ComPtr<ID3D11CommandList> recorded;
				require(SUCCEEDED(analysis.FinishCommandList(FALSE, &recorded)), "analysis recording failed");
				analysis_probe_enabled = true;
				playback.ExecuteCommandList(recorded.Get(), TRUE);
			}
		}
		require(analysis_probe_seen, "analysis dump did not reach the controlled input boundary");
		if (analysis_reload_error)
			std::rethrow_exception(analysis_reload_error);
		require(G->gReloadConfigPending && command_list_globals.at(L"$known").fval == 11.0f,
		        "analysis input lost its pending assignment across reload");
		require(file_contents(files.user).find("$known = 7") != std::string::npos,
		        "reload saved or replaced configuration while analysis owned its global lock");
		fixture.require_no_errors();
		ReloadConfig(&device);
		require(command_list_globals.at(L"$known").fval == 11.0f && !G->gReloadConfigPending,
		        "analysis reload did not complete after the dump finished");
	}
}

void preserves_only_eligible_settings_across_reload()
{
	D3DFixture fixture;
	ConfigFixture resources;
	SettingsConfigFixture files;
	std::ofstream(files.user) << "[Constants]\n$known = 7\n$unknown = 9\n$temporary = 99\n";
	auto load = [&]()
	{
		LoadConfigFile();
		for (const auto &command : G->constants_command_list.commands)
			command->run(&fixture.state);
	};
	load();
	require(command_list_globals.at(L"$known").fval == 7.0f, "known persistent value was not restored");
	require(command_list_globals.at(L"$temporary").fval == 5.0f, "old value was restored after persist was removed");
	G->user_config_dirty |= 1;
	require(SavePersistentSettings(), "first settings save failed");
	auto saved = file_contents(files.user);
	require(saved.find("$unknown = 9") != std::string::npos, "first load did not preserve unknown settings");
	require(saved.find("$temporary") == std::string::npos, "nonpersistent setting was retained on disk");
	std::ofstream(files.config, std::ios::app) << "global persist $unknown = 2\n";
	load();
	require(command_list_globals.at(L"$unknown").fval == 9.0f, "returning persistent variable lost its saved value");
	require(SavePersistentSettings(), "save after returning variable failed");
	files.write_config(true);
	load();
	G->user_config_dirty |= 1;
	require(SavePersistentSettings(), "save after removing variable failed");
	std::ofstream(files.user) << "[Constants]\n$known = 7\n$replacement = 9\n";
	load();
	G->user_config_dirty |= 1;
	require(SavePersistentSettings(), "renamed-unknown save failed");
	require(file_contents(files.user).find("$replacement = 9") != std::string::npos,
	        "changed unknown name with the same count lost its grace period");
	std::ofstream(files.user) << "[Constants]\n$known = 7\n$replacement = 10\n";
	load();
	G->user_config_dirty |= 1;
	require(SavePersistentSettings(), "changed-unknown save failed");
	require(file_contents(files.user).find("$replacement = 10") != std::string::npos,
	        "changed unknown value with the same count lost its grace period");
	load();
	require(SavePersistentSettings(), "stale-unknown cleanup failed");
	require(file_contents(files.user).find("$replacement") == std::string::npos,
	        "unchanged unknown survived its grace period");
	files.write_config(false);
	std::ofstream(files.user) << "[Constants]\n$known = 7\n$unknown = 11\n";
	load();
	load();
	G->user_config_dirty |= 1;
	require(SavePersistentSettings(), "retained-unknown save failed");
	require(file_contents(files.user).find("$unknown = 11") != std::string::npos,
	        "cleanup-disabled policy lost unknown settings");
	require(WipeUserConfig(), "explicit wipe failed");
	require(!std::filesystem::exists(files.user), "wipe left the user configuration file");
	G->user_config_dirty |= 1;
	require(SavePersistentSettings(), "save after wipe failed");
	require(file_contents(files.user).find("$unknown") == std::string::npos, "wipe left cached unknown settings");
	fixture.require_no_errors();
}

void preserves_constants_declaration_and_execution_order()
{
	D3DFixture fixture;
	ConfigFixture resources;
	SettingsConfigFixture files;
	std::ofstream(files.config) << "[Logging]\nenabled=0\ncalls=0\nshow_warnings=0\n"
	                               "[Include]\nuser_config=runtime-user.ini\n[Constants]\n"
	                               "$second = $first + 1\nglobal $first = 2\nglobal $second = 0\n"
	                               "$first = 5\n$second = $second + 10\nglobal persist $third = 3\n"
	                               "global $first = 99\nglobal $bad-name = 4\n";
	LoadConfigFile();
	require(command_list_globals.at(L"$first").fval == 2.0f, "duplicate declaration replaced the first initializer");
	require(command_list_globals.find(L"$bad-name") == command_list_globals.end(), "invalid declaration was accepted");
	for (const auto &command : G->constants_command_list.commands)
		command->run(&fixture.state);
	require(command_list_globals.at(L"$first").fval == 5.0f, "declaration compaction changed assignments");
	require(command_list_globals.at(L"$second").fval == 13.0f, "compaction changed execution/declaration ordering");
	require(command_list_globals.at(L"$third").fval == 3.0f, "compaction discarded a trailing declaration");
}

void preserves_duplicate_sections_and_recursive_includes()
{
	ConfigFixture resources;
	SettingsConfigFixture files;
	const std::string prefix =
	    "[Logging]\nenabled=0\ncalls=0\nshow_warnings=0\n[Include]\nuser_config=runtime-user.ini\n";
	const std::string device = "[Device]\nwidth=100\nwidth=200\n[Device]\nheight=300\n";
	std::ofstream(files.config) << prefix << device;
	LoadConfigFile();
	require(GetIniInt(L"Device", L"width", -1, nullptr) == 100, "duplicate key replaced the first value");
	require(GetIniInt(L"Device", L"height", -1, nullptr) == -1, "duplicate section reused the previous section");
	const auto child = files.config.parent_path() / L"runtime-child.ini";
	const auto grandchild = files.config.parent_path() / L"runtime-grandchild.ini";
	require(!std::filesystem::exists(child) && !std::filesystem::exists(grandchild), "include fixtures already exist");
	files.includes = {child, grandchild};
	std::ofstream(child) << "[Include]\ninclude=runtime-grandchild.ini\n[Device]\nwidth=400\n";
	std::ofstream(grandchild) << "[Device]\nwidth=500\n";
	std::ofstream(files.config) << prefix << "include=runtime-child.ini\n" << device;
	LoadConfigFile();
	require(GetIniInt(L"Device", L"width", -1, nullptr) == 500, "recursive override lost its final value");
}

void preserves_static_and_dynamic_conditions_across_reload()
{
	D3DFixture fixture;
	ConfigFixture resources;
	SettingsConfigFixture files;
	auto write = [&](bool condition)
	{
		std::ofstream(files.config) << "[Logging]\nenabled=0\ncalls=0\nshow_warnings=0\n"
		                               "[Include]\nuser_config=runtime-user.ini\n[Constants]\n"
		                               "global $static_result=0\nglobal $dynamic_result=0\nglobal $gate=0\nif "
		                            << (condition ? "1" : "0")
		                            << "\n$static_result=2\nelse\n$static_result=3\nendif\n"
		                               "if $gate\n$dynamic_result=4\nelse\n$dynamic_result=5\nendif\n";
	};
	auto run = [&]()
	{
		for (const auto &command : G->constants_command_list.commands)
			command->run(&fixture.state);
	};
	write(true);
	LoadConfigFile();
	optimise_command_lists(nullptr);
	run();
	require(command_list_globals.at(L"$static_result").fval == 2.0f, "static true condition chose the wrong branch");
	require(command_list_globals.at(L"$dynamic_result").fval == 5.0f, "dynamic false condition chose the wrong branch");
	command_list_globals.at(L"$gate").fval = 1.0f;
	run();
	require(command_list_globals.at(L"$dynamic_result").fval == 4.0f, "optimizer froze a dynamic condition");
	write(false);
	LoadConfigFile();
	optimise_command_lists(nullptr);
	run();
	require(command_list_globals.at(L"$static_result").fval == 3.0f,
	        "new configuration reused the previous static condition");
	require(command_list_globals.at(L"$dynamic_result").fval == 5.0f,
	        "reload did not reset the dynamic condition state");
}

// Follow-up cases use the same production parser/interpreter and real D3D boundary.
void preserves_expression_operator_contract()
{
	const struct
	{
		const wchar_t *text;
		float expected;
	} cases[] = {{L"2 + 3 * 4", 14},
	             {L"20 - 6 - 3", 11},
	             {L"20 / 2 / 2", 5},
	             {L"2 ** 3 ** 2", 512},
	             {L"-2 ** 2", 4},
	             {L"!0 + ~0", 0},
	             {L"17 // 3 + 17 % 3", 7},
	             {L"1 + 2 << 2", 12},
	             {L"32 >> 2 + 1", 4},
	             {L"2 < 3 == 1", 1},
	             {L"2 <= 2 && 4 >= 3 && 5 > 4", 1},
	             {L"1 | 6 ^ 3 & 1", 7},
	             {L"0 && 1 || 1", 1},
	             {L"0 === -0", 0},
	             {L"0 !== -0", 1},
	             {L"2 != 3", 1},
	             {L"abs(-3) + sqrt(16)", 7},
	             {L"((2 + 3) * (7 - 4))", 15}};
	for (const auto &item : cases)
		require(evaluate(item.text) == item.expected, "expression changed documented operator behavior");
	for (const wchar_t *text : {L"1 +", L"(1 + 2", L"1 2", L"sqrt()", L"2 *** 3", L"1 + )2("})
	{
		CommandListExpression expression;
		const std::wstring input(text);
		require(!expression.parse(&input, nullptr, nullptr), "malformed expression was accepted");
	}
	ConfigFixture resources;
	command_list_globals.emplace(L"$\\test.ini\\value",
	                             CommandListVariable(L"$\\test.ini\\value", 3, VariableFlags::NONE));
	CommandListExpression expression;
	const std::wstring input = L"($\\test.ini\\value + 2) * 3";
	require(expression.parse(&input, &config_namespace, nullptr), "namespaced expression was rejected");
	CommandListState state;
	require(expression.evaluate(&state) == 15, "namespaced expression returned the wrong value");
	command_list_globals.at(L"$\\test.ini\\value").fval = 5;
	require(expression.evaluate(&state) == 21, "parsed expression froze its variable value");
}

struct ProfilingFixture
{
	bool implicit_post = G->implicit_post_checktextureoverride_used;
	std::wstring warning = Profiling::cto_warning;
	ProfilingFixture()
	{
		Profiling::mode = Profiling::Mode::NONE;
		Profiling::clear();
	}
	~ProfilingFixture()
	{
		Profiling::mode = Profiling::Mode::NONE;
		Profiling::clear();
		Profiling::text.clear();
		G->implicit_post_checktextureoverride_used = implicit_post;
		Profiling::cto_warning = warning;
	}
};

BOOL WINAPI publication_counter(LARGE_INTEGER *value)
{
	value->QuadPart = 100;
	return TRUE;
}

void empty_entry_paths_preserve_later_work_and_profiling()
{
	IniParamsFixture fixture;
	CommandListVariable result(L"$entry", 0.0f, VariableFlags::NONE);
	CommandList empty, generated, proxy, other;
	generated.runtime_populated = true;
	require(proxy.SetSourceCommandList(&generated), "empty proxy was rejected");
	auto first = std::make_shared<VariableAssignment>();
	first->var = &result;
	const std::wstring seven = L"7";
	require(first->expression.parse(&seven, nullptr, nullptr), "first assignment was rejected");
	auto second = std::make_shared<VariableAssignment>();
	second->var = &result;
	const std::wstring eleven = L"11";
	require(second->expression.parse(&eleven, nullptr, nullptr), "second assignment was rejected");
	other.commands.push_back(second);
	ProfilingFixture profiling;
	ImportBoundary clock(GetModuleHandleW(nullptr), "QueryPerformanceCounter", &publication_counter);
	Profiling::mode = Profiling::Mode::TOP_COMMAND_LISTS;
	RunCommandList(&fixture.device, fixture.d3d.hacker_context.get(), &empty, nullptr, false);
	RunCommandList(&fixture.device, fixture.d3d.hacker_context.get(), &proxy, nullptr, false);
	require(result.fval == 0.0f, "empty command list changed its output");
	require(command_lists_profiling.empty() && command_lists_cmd_profiling.empty(),
	        "empty invocation invented profiler work");
	generated.commands.push_back(first);
	RunCommandList(&fixture.device, fixture.d3d.hacker_context.get(), &proxy, nullptr, false);
	require(result.fval == 7.0f, "populated command list retained an empty classification");
	require(generated.executions == 1, "populated command list lost or duplicated its profile measurement");
	require(proxy.SetSourceCommandList(&other), "retargeted proxy was rejected");
	RunCommandList(&fixture.device, fixture.d3d.hacker_context.get(), &proxy, nullptr, false);
	require(result.fval == 11.0f, "proxy retained its previous resolved target");
	require(other.executions == 1 && command_lists_profiling.size() == 2, "proxy changed profiler measurements");
	empty.commands.push_back(first);
	RunCommandList(&fixture.device, fixture.d3d.hacker_context.get(), &empty, nullptr, false);
	require(result.fval == 7.0f, "ordinary list retained an empty classification");
	require(empty.executions == 1 && command_lists_profiling.size() == 3, "new work lost its profiler measurement");
	fixture.d3d.require_no_errors();
}

void write_recursive_profile_config(SettingsConfigFixture &files)
{
	std::ofstream(files.config)
	    << "[Logging]\nenabled=0\ncalls=0\nshow_warnings=0\n"
	       "[Include]\nuser_config=runtime-user.ini\n[Constants]\nglobal $visits=0\n"
	       "[CommandListRecursive]\n$visits=$visits+1\nif $visits < 3\nrun=CommandListRecursive\nendif\n";
	LoadConfigFile();
}

void runs_recursive_profile_list(CommandListState &state)
{
	command_list_globals.at(L"$visits").fval = 0;
	RunLinkedCommandList runner(&explicitCommandListSections.at(L"commandlistrecursive").command_list);
	runner.run(&state);
	require(command_list_globals.at(L"$visits").fval == 3, "profiling changed recursive execution");
}

void profiling_collections_follow_mode_and_window_changes()
{
	D3DFixture fixture;
	ConfigFixture resources;
	SettingsConfigFixture files;
	ProfilingFixture profiling;
	write_recursive_profile_config(files);
	for (auto expected : {Profiling::Mode::SUMMARY, Profiling::Mode::TOP_COMMAND_LISTS, Profiling::Mode::TOP_COMMANDS,
	                      Profiling::Mode::NONE})
	{
		Profiling::next_mode();
		require(Profiling::mode == expected, "monitor mode cycle changed");
		require(command_lists_profiling.empty() && command_lists_cmd_profiling.empty(),
		        "mode switch retained a collection");
		runs_recursive_profile_list(fixture.state);
		runs_recursive_profile_list(fixture.state);
		if (expected == Profiling::Mode::SUMMARY || expected == Profiling::Mode::TOP_COMMAND_LISTS)
		{
			require(command_lists_profiling.size() == 2 && command_lists_cmd_profiling.empty(),
			        "wrong list collection for selected mode");
			require(explicitCommandListSections.at(L"commandlistrecursive").command_list.executions == 6,
			        "recursive/repeated list executions were lost or counted twice");
		}
		else if (expected == Profiling::Mode::TOP_COMMANDS)
		{
			require(command_lists_cmd_profiling.size() == 3 && command_lists_profiling.empty(),
			        "wrong command collection for selected mode");
			unsigned executions = 0;
			for (auto *command : command_lists_cmd_profiling)
				executions += command->pre_executions;
			require(executions == 16, "recursive/repeated command executions changed");
		}
		else
			require(command_lists_profiling.empty() && command_lists_cmd_profiling.empty(),
			        "disabled monitor collected commands");
		Profiling::clear();
		require(command_lists_profiling.empty() && command_lists_cmd_profiling.empty(), "clear retained a collection");
		runs_recursive_profile_list(fixture.state);
		if (expected == Profiling::Mode::SUMMARY || expected == Profiling::Mode::TOP_COMMAND_LISTS)
			require(explicitCommandListSections.at(L"commandlistrecursive").command_list.executions == 3,
			        "new window inherited the previous execution count");
		if (expected == Profiling::Mode::TOP_COMMANDS)
		{
			unsigned executions = 0;
			for (auto *command : command_lists_cmd_profiling)
				executions += command->pre_executions;
			require(executions == 8, "new window inherited command counts");
			Profiling::clear();
			CommandList post;
			auto shared = explicitCommandListSections.at(L"commandlistrecursive").command_list.commands[0];
			post.commands.push_back(shared);
			RunLinkedCommandList post_runner(&post);
			fixture.state.post = true;
			post_runner.run(&fixture.state);
			post_runner.run(&fixture.state);
			fixture.state.post = false;
			require(shared->pre_executions == 0 && shared->post_executions == 2 &&
			            command_lists_cmd_profiling.size() == 1,
			        "shared post commands lost phase counts or registered more than once");
		}
	}
	G->implicit_post_checktextureoverride_used = true;
	Profiling::cto_warning = L"test warning";
	for (int i = 0; i < 4; ++i)
		Profiling::next_mode();
	require(Profiling::mode == Profiling::Mode::CTO_WARNING, "applicable warning mode was skipped");
	runs_recursive_profile_list(fixture.state);
	require(command_lists_profiling.empty() && command_lists_cmd_profiling.empty(), "warning mode collected commands");
	Profiling::next_mode();
	require(Profiling::mode == Profiling::Mode::NONE, "warning mode did not cycle to off");
}

void profiling_reload_discards_old_command_collections()
{
	D3DFixture fixture;
	ConfigFixture resources;
	SettingsConfigFixture files;
	ProfilingFixture profiling;
	write_recursive_profile_config(files);
	for (auto mode : {Profiling::Mode::SUMMARY, Profiling::Mode::TOP_COMMANDS})
	{
		Profiling::mode = mode;
		Profiling::clear();
		runs_recursive_profile_list(fixture.state);
		Profiling::text = L"previous configuration";
		LoadConfigFile();
		require(command_lists_profiling.empty() && command_lists_cmd_profiling.empty() && Profiling::text.empty(),
		        "reload retained command identities or display text from old storage");
		runs_recursive_profile_list(fixture.state);
		if (mode == Profiling::Mode::SUMMARY)
			require(explicitCommandListSections.at(L"commandlistrecursive").command_list.executions == 3,
			        "reloaded list did not start a fresh collection");
		else
			require(command_lists_cmd_profiling.size() == 3, "reloaded commands were not registered");
	}
}

void matching_diagnostics_preserve_hash_first_and_order()
{
	D3DFixture fixture;
	ProfilingFixture profiling;
	struct Cleanup
	{
		~Cleanup()
		{
			G->mFuzzyTextureOverrides.clear();
			G->mTextureOverrideMap.clear();
			G->mResources.clear();
			G->fuzzy_match_alongside_hash = false;
		}
	} cleanup;
	G->mFuzzyTextureOverrides.clear();
	G->mTextureOverrideMap.clear();
	G->mResources.clear();
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = 32;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer, miss;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, nullptr, &buffer)), "matching buffer creation failed");
	desc.ByteWidth = 64;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, nullptr, &miss)), "matching miss buffer creation failed");
	G->mResources[buffer.Get()].hash = 0x1234;
	auto &exact = G->mTextureOverrideMap[0x1234];
	exact.emplace_back();
	exact[0].ini_section = L"hash";
	for (const wchar_t *name : {L"z", L"b", L"a", L"miss"})
	{
		auto match = std::make_shared<FuzzyMatchResourceDesc>(name);
		match->set_resource_type(D3D11_RESOURCE_DIMENSION_BUFFER);
		match->ByteWidth.op = FuzzyMatchOp::EQUAL;
		match->ByteWidth.val = std::wstring(name) == L"miss" ? 128 : 32;
		match->texture_override->priority = std::wstring(name) == L"z" ? 10 : 0;
		G->mFuzzyTextureOverrides.insert(match);
	}
	for (auto mode : {Profiling::Mode::NONE, Profiling::Mode::SUMMARY, Profiling::Mode::TOP_COMMAND_LISTS,
	                  Profiling::Mode::TOP_COMMANDS, Profiling::Mode::CTO_WARNING})
	{
		Profiling::mode = mode;
		Profiling::clear();
		TextureOverrideMatches matches;
		G->fuzzy_match_alongside_hash = false;
		find_texture_overrides_for_resource(buffer.Get(), &matches, nullptr);
		require(matches.size() == 1 && matches[0]->ini_section == L"hash",
		        "hash-first matching included fuzzy overrides");
		G->fuzzy_match_alongside_hash = true;
		matches.clear();
		find_texture_overrides_for_resource(buffer.Get(), &matches, nullptr);
		require(matches.size() == 4 && matches[0]->ini_section == L"hash" && matches[1]->ini_section == L"a" &&
		            matches[2]->ini_section == L"b" && matches[3]->ini_section == L"z",
		        "dual matching changed ordered results");
		G->mResources.erase(buffer.Get());
		matches.clear();
		find_texture_overrides_for_resource(buffer.Get(), &matches, nullptr);
		require(matches.size() == 3 && matches[0]->ini_section == L"a" && matches[1]->ini_section == L"b" &&
		            matches[2]->ini_section == L"z",
		        "untracked resource lost ordered descriptor matches");
		matches.clear();
		find_texture_overrides_for_resource(miss.Get(), &matches, nullptr);
		require(matches.empty(), "unmatched resource produced a match");
		const bool collecting = mode == Profiling::Mode::SUMMARY;
		require(Profiling::texture_override_match_overhead.count == (collecting ? 4U : 0U) &&
		            Profiling::texture_override_match_overhead.hits == (collecting ? 3U : 0U) &&
		            Profiling::texture_override_fuzzy_match_overhead.count == (collecting ? 3U : 0U) &&
		            Profiling::texture_override_fuzzy_match_overhead.hits == (collecting ? 2U : 0U) &&
		            Profiling::fuzzy_candidates_tested == (collecting ? 12U : 0U),
		        "matching diagnostics collected wrong counts or mode");
		if (!collecting)
			require(Profiling::texture_override_match_overhead.cpu.QuadPart == 0 &&
			            Profiling::texture_override_fuzzy_match_overhead.cpu.QuadPart == 0,
			        "inactive matching diagnostics collected CPU time");
		Profiling::clear();
		require(Profiling::texture_override_match_overhead.count == 0 &&
		            Profiling::texture_override_fuzzy_match_overhead.count == 0 &&
		            Profiling::fuzzy_candidates_tested == 0,
		        "clear retained matching metrics");
		G->mResources[buffer.Get()].hash = 0x1234;
	}
	fixture.require_no_errors();
}

Microsoft::WRL::ComPtr<ID3D11View> bound_view(D3DFixture &fixture, ResourceCopyTargetType kind)
{
	switch (kind)
	{
	case ResourceCopyTargetType::SHADER_RESOURCE:
	{
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> v;
		fixture.context->PSGetShaderResources(0, 1, &v);
		return v;
	}
	case ResourceCopyTargetType::RENDER_TARGET:
	{
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> v;
		fixture.context->OMGetRenderTargets(1, &v, nullptr);
		return v;
	}
	case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
	{
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> v;
		fixture.context->OMGetRenderTargets(0, nullptr, &v);
		return v;
	}
	case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
	{
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> v;
		fixture.context->CSGetUnorderedAccessViews(0, 1, &v);
		return v;
	}
	default:
		throw std::runtime_error("unsupported fixture binding");
	}
}

void require_view_resource(ID3D11View *view, ID3D11Resource *expected)
{
	require(view != nullptr, "expected pipeline view is missing");
	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	view->GetResource(&resource);
	require(resource.Get() == expected, "view refers to the wrong resource");
}

std::shared_ptr<std::atomic_bool> track_view_lifetime(ID3D11View *view)
{
	auto destroyed = std::make_shared<std::atomic_bool>(false);
	auto *lifetime = new ViewLifetime(destroyed);
	const GUID key = {0x5b6dce93, 0x77c8, 0x4d4c, {0xb4, 0x96, 0x37, 0xc1, 0x83, 0xcd, 0x86, 0xf4}};
	require(SUCCEEDED(view->SetPrivateDataInterface(key, lifetime)), "could not track copied view lifetime");
	lifetime->Release();
	return destroyed;
}

struct TextureViewFixture
{
	std::array<Microsoft::WRL::ComPtr<ID3D11Texture2D>, 2> resources;
	std::array<Microsoft::WRL::ComPtr<ID3D11View>, 2> views;
	TextureViewFixture(D3DFixture &fixture, ResourceCopyTargetType kind)
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = desc.Height = 4;
		desc.MipLevels = 2;
		desc.ArraySize = desc.SampleDesc.Count = 1;
		desc.Format =
		    kind == ResourceCopyTargetType::DEPTH_STENCIL_TARGET ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_R32_FLOAT;
		switch (kind)
		{
		case ResourceCopyTargetType::SHADER_RESOURCE:
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			break;
		case ResourceCopyTargetType::RENDER_TARGET:
			desc.BindFlags = D3D11_BIND_RENDER_TARGET;
			break;
		case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
			desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			break;
		case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
			desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
			break;
		default:
			throw std::runtime_error("unsupported fixture view");
		}
		for (size_t i = 0; i < 2; ++i)
		{
			require(SUCCEEDED(fixture.device->CreateTexture2D(&desc, nullptr, &resources[i])),
			        "texture view resource creation failed");
			switch (kind)
			{
			case ResourceCopyTargetType::SHADER_RESOURCE:
			{
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> v;
				require(SUCCEEDED(fixture.device->CreateShaderResourceView(resources[i].Get(), nullptr, &v)),
				        "source SRV failed");
				views[i] = v;
				break;
			}
			case ResourceCopyTargetType::RENDER_TARGET:
			{
				Microsoft::WRL::ComPtr<ID3D11RenderTargetView> v;
				require(SUCCEEDED(fixture.device->CreateRenderTargetView(resources[i].Get(), nullptr, &v)),
				        "source RTV failed");
				views[i] = v;
				break;
			}
			case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
			{
				Microsoft::WRL::ComPtr<ID3D11DepthStencilView> v;
				require(SUCCEEDED(fixture.device->CreateDepthStencilView(resources[i].Get(), nullptr, &v)),
				        "source DSV failed");
				views[i] = v;
				break;
			}
			case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
			{
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> v;
				require(SUCCEEDED(fixture.device->CreateUnorderedAccessView(resources[i].Get(), nullptr, &v)),
				        "source UAV failed");
				views[i] = v;
				break;
			}
			default:
				break;
			}
		}
	}
};

void reuses_matching_views_with_owned_lifetimes()
{
	D3DFixture fixture;
	ProfilingFixture profiling;
	Microsoft::WRL::ComPtr<D3DDeviceBoundary> boundary;
	boundary.Attach(new D3DDeviceBoundary(fixture.device.Get()));
	fixture.state.mOrigDevice1 = boundary.Get();
	for (auto kind : {ResourceCopyTargetType::SHADER_RESOURCE, ResourceCopyTargetType::RENDER_TARGET,
	                  ResourceCopyTargetType::DEPTH_STENCIL_TARGET, ResourceCopyTargetType::UNORDERED_ACCESS_VIEW})
	{
		for (auto mode : {Profiling::Mode::NONE, Profiling::Mode::SUMMARY, Profiling::Mode::TOP_COMMAND_LISTS,
		                  Profiling::Mode::TOP_COMMANDS})
		{
			Profiling::mode = mode;
			Profiling::clear();
			fixture.context->ClearState();
			TextureViewFixture input(fixture, kind);
			auto first = track_view_lifetime(input.views[0].Get());
			auto last = track_view_lifetime(input.views[1].Get());
			{
				CustomResource source;
				ResourceCopyOperation operation;
				operation.options = ResourceCopyOptions::REFERENCE;
				operation.src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
				operation.src.SetCustomResource(&source);
				operation.dst.type = kind;
				operation.dst.shader_type = kind == ResourceCopyTargetType::UNORDERED_ACCESS_VIEW ? L'c' : L'p';
				boundary->view_creations = 0;
				for (size_t index : {size_t{0}, size_t{1}, size_t{0}, size_t{1}})
				{
					operation.CopyResourceToResource(
					    &fixture.state, input.resources[index].Get(), input.views[index].Get(), 0, 0,
					    kind == ResourceCopyTargetType::DEPTH_STENCIL_TARGET ? DXGI_FORMAT_D32_FLOAT
						                                                     : DXGI_FORMAT_R32_FLOAT,
					    0);
					auto bound = bound_view(fixture, kind);
					require_view_resource(bound.Get(), input.resources[index].Get());
					if (kind == ResourceCopyTargetType::SHADER_RESOURCE)
					{
						Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> typed;
						require(SUCCEEDED(bound.As(&typed)), "bound view lacks SRV interface");
						D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
						typed->GetDesc(&desc);
						require(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D && desc.Texture2D.MipLevels == 2,
						        "reused SRV lost mip range");
					}
				}
				operation.CopyResourceToResource(&fixture.state, input.resources[1].Get(), input.views[1].Get(), 0, 0,
				                                 kind == ResourceCopyTargetType::DEPTH_STENCIL_TARGET
				                                     ? DXGI_FORMAT_D32_FLOAT
													 : DXGI_FORMAT_R32_FLOAT,
				                                 0);
				require(Profiling::source_views_reused == (mode == Profiling::Mode::SUMMARY ? 4U : 0U) &&
				            Profiling::cached_views_reused == (mode == Profiling::Mode::SUMMARY ? 1U : 0U),
				        "view reuse diagnostics changed mode or source/cache counts");
				require(boundary->view_creations == 0, "matching views were recreated instead of reused");
				input.views[0].Reset();
				input.views[1].Reset();
				fixture.context->ClearState();
				fixture.context->Flush();
				require(first->load() && !last->load(), "view cache has incorrect owned lifetime");
			}
			fixture.context->Flush();
			require(last->load(), "view cache leaked its queried interface");
		}
	}
	fixture.require_no_errors();
}

struct BufferViewFixture
{
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer, other;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
	BufferViewFixture(D3DFixture &fixture)
	{
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = 32;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		const float values[] = {1, 2, 3, 4, 5, 6, 7, 8};
		D3D11_SUBRESOURCE_DATA initial{};
		initial.pSysMem = values;
		require(SUCCEEDED(fixture.device->CreateBuffer(&desc, &initial, &buffer)),
		        "buffer view source creation failed");
		require(SUCCEEDED(fixture.device->CreateBuffer(&desc, &initial, &other)), "other view source creation failed");
		D3D11_SHADER_RESOURCE_VIEW_DESC vd{};
		vd.Format = DXGI_FORMAT_R32_FLOAT;
		vd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		vd.Buffer.NumElements = 8;
		require(SUCCEEDED(fixture.device->CreateShaderResourceView(buffer.Get(), &vd, &view)),
		        "buffer source view creation failed");
	}
};

void require_buffer_view(D3DFixture &fixture, ID3D11Resource *resource, DXGI_FORMAT format, UINT first, UINT count,
                         bool raw = false)
{
	auto bound = bound_view(fixture, ResourceCopyTargetType::SHADER_RESOURCE);
	require_view_resource(bound.Get(), resource);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> typed;
	require(SUCCEEDED(bound.As(&typed)), "buffer view lacks SRV interface");
	D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
	typed->GetDesc(&desc);
	require(desc.Format == format &&
	            desc.ViewDimension == (raw ? D3D11_SRV_DIMENSION_BUFFEREX : D3D11_SRV_DIMENSION_BUFFER),
	        "view changed required format or dimension");
	require(desc.Buffer.FirstElement == first && desc.Buffer.NumElements == count,
	        "view exposes the wrong buffer range");
	if (raw)
		require(desc.BufferEx.Flags == D3D11_BUFFEREX_SRV_FLAG_RAW, "raw view lost its raw flag");
}

void incompatible_views_use_correct_factory_descriptors()
{
	D3DFixture fixture;
	BufferViewFixture input(fixture);
	CustomResource source;
	Microsoft::WRL::ComPtr<D3DDeviceBoundary> boundary;
	boundary.Attach(new D3DDeviceBoundary(fixture.device.Get()));
	fixture.state.mOrigDevice1 = boundary.Get();
	for (int example = 0; example < 4; ++example)
	{
		ResourceCopyOperation operation;
		operation.options = ResourceCopyOptions::REFERENCE;
		operation.src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
		operation.src.SetCustomResource(&source);
		operation.dst.type = ResourceCopyTargetType::SHADER_RESOURCE;
		operation.dst.shader_type = L'p';
		boundary->view_creations = 0;
		auto *resource = example == 1 ? input.other.Get() : input.buffer.Get();
		if (example == 3)
			operation.options |= ResourceCopyOptions::RAW_VIEW;
		operation.CopyResourceToResource(&fixture.state, resource, input.view.Get(), 4, example == 0 ? 8 : 0,
		                                 example == 2 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R32_FLOAT,
		                                 example == 0 ? 24 : 32);
		fixture.require_no_errors();
		require_buffer_view(fixture, resource,
		                    example == 3   ? DXGI_FORMAT_R32_TYPELESS
		                    : example == 2 ? DXGI_FORMAT_R32_UINT
		                                   : DXGI_FORMAT_R32_FLOAT,
		                    example == 0 ? 2 : 0, example == 0 ? 4 : 8, example == 3);
		require(boundary->view_creations == 1, "incompatible source view bypassed required view creation");
	}
	{
		fixture.context->ClearState();
		ResourceCopyTarget current;
		current.type = ResourceCopyTargetType::UNORDERED_ACCESS_VIEW;
		current.shader_type = L'c';
		fixture.state.this_target = &current;
		ResourceCopyOperation operation;
		operation.options = ResourceCopyOptions::REFERENCE;
		operation.src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
		operation.src.SetCustomResource(&source);
		operation.dst.type = ResourceCopyTargetType::THIS_RESOURCE;
		boundary->view_creations = 0;
		operation.CopyResourceToResource(&fixture.state, input.buffer.Get(), input.view.Get(), 4, 0,
		                                 DXGI_FORMAT_R32_FLOAT, 32);
		auto bound = bound_view(fixture, ResourceCopyTargetType::UNORDERED_ACCESS_VIEW);
		require_view_resource(bound.Get(), input.buffer.Get());
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> typed;
		require(SUCCEEDED(bound.As(&typed)), "SRV source bypassed required UAV interface");
		D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
		typed->GetDesc(&desc);
		require(desc.Format == DXGI_FORMAT_R32_FLOAT && desc.Buffer.NumElements == 8 && desc.Buffer.Flags == 0,
		        "interface fallback changed UAV descriptor");
		fixture.context->ClearState();
		current.type = ResourceCopyTargetType::SHADER_RESOURCE;
		current.shader_type = L'p';
		operation.CopyResourceToResource(&fixture.state, input.buffer.Get(), input.view.Get(), 4, 0,
		                                 DXGI_FORMAT_R32_FLOAT, 32);
		require_buffer_view(fixture, input.buffer.Get(), DXGI_FORMAT_R32_FLOAT, 0, 8);
		require(boundary->view_creations == 1,
		        "changing this target reused a wrong-kind view or recreated a compatible source");
		fixture.state.this_target = nullptr;
	}
	CustomResource destination;
	destination.override_format = DXGI_FORMAT_R32_UINT;
	destination.override_stride = 8;
	ResourceCopyOperation operation;
	operation.options = ResourceCopyOptions::REFERENCE;
	operation.src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
	operation.src.SetCustomResource(&source);
	operation.dst.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
	operation.dst.SetCustomResource(&destination);
	operation.CopyResourceToResource(&fixture.state, input.buffer.Get(), input.view.Get(), 4, 0, DXGI_FORMAT_R32_FLOAT,
	                                 32);
	// A custom resource may retain metadata without a view until a concrete
	// pipeline target requests its interface. Observe that actual binding.
	ResourceCopyOperation bind;
	bind.options = ResourceCopyOptions::REFERENCE;
	bind.src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
	bind.src.SetCustomResource(&destination);
	bind.dst.type = ResourceCopyTargetType::SHADER_RESOURCE;
	bind.dst.shader_type = L'p';
	fixture.context->ClearState();
	bind.run(&fixture.state);
	require_buffer_view(fixture, input.buffer.Get(), DXGI_FORMAT_R32_UINT, 0, 8);
	auto final_view = bound_view(fixture, ResourceCopyTargetType::SHADER_RESOURCE);

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> typed;
	require(SUCCEEDED(final_view->QueryInterface(IID_PPV_ARGS(&typed))), "override view lacks required interface");
	D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
	typed->GetDesc(&desc);
	require(desc.Format == DXGI_FORMAT_R32_UINT && desc.Buffer.NumElements == 8 && destination.stride == 8,
	        "custom destination override was bypassed");
	fixture.require_no_errors();
}

void custom_references_retain_only_destination_ownership()
{
	D3DFixture fixture;
	BufferViewFixture input(fixture);
	auto destroyed = track_view_lifetime(input.view.Get());
	{
		CustomResource destination;
		ResourceCopyOperation operation;
		operation.options = ResourceCopyOptions::REFERENCE;
		operation.src.type = ResourceCopyTargetType::SHADER_RESOURCE;
		operation.src.shader_type = L'p';
		operation.dst.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
		operation.dst.SetCustomResource(&destination);
		for (int i = 0; i < 3; ++i)
			operation.CopyResourceToResource(&fixture.state, input.buffer.Get(), input.view.Get(), 4, 0,
			                                 DXGI_FORMAT_R32_FLOAT, 32);
		require_view_resource(destination.view, input.buffer.Get());
		input.view.Reset();
		fixture.context->ClearState();
		fixture.context->Flush();
		require(!destroyed->load(), "custom destination did not retain its source view");
	}
	fixture.context->Flush();
	require(destroyed->load(), "custom reference leaked an owned queried view");
	fixture.require_no_errors();
}

void cached_views_follow_changing_source_metadata()
{
	D3DFixture fixture;
	BufferViewFixture input(fixture);
	CustomResource source;
	ResourceCopyOperation operation;
	operation.options = ResourceCopyOptions::REFERENCE;
	operation.src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
	operation.src.SetCustomResource(&source);
	operation.dst.type = ResourceCopyTargetType::SHADER_RESOURCE;
	operation.dst.shader_type = L'p';
	operation.CopyResourceToResource(&fixture.state, input.buffer.Get(), input.view.Get(), 4, 0, DXGI_FORMAT_R32_FLOAT,
	                                 32);
	require_buffer_view(fixture, input.buffer.Get(), DXGI_FORMAT_R32_FLOAT, 0, 8);
	operation.CopyResourceToResource(&fixture.state, input.buffer.Get(), input.view.Get(), 4, 8, DXGI_FORMAT_R32_FLOAT,
	                                 24);
	require_buffer_view(fixture, input.buffer.Get(), DXGI_FORMAT_R32_FLOAT, 2, 4);
	operation.CopyResourceToResource(&fixture.state, input.buffer.Get(), input.view.Get(), 4, 0, DXGI_FORMAT_R32_UINT,
	                                 32);
	require_buffer_view(fixture, input.buffer.Get(), DXGI_FORMAT_R32_UINT, 0, 8);
	fixture.require_no_errors();
}

void failed_view_creation_preserves_bindings_and_selected_metrics()
{
	D3DFixture fixture;
	ProfilingFixture profiling;
	Microsoft::WRL::ComPtr<D3DDeviceBoundary> boundary;
	boundary.Attach(new D3DDeviceBoundary(fixture.device.Get()));
	fixture.state.mOrigDevice1 = boundary.Get();
	for (auto kind : {ResourceCopyTargetType::SHADER_RESOURCE, ResourceCopyTargetType::RENDER_TARGET,
	                  ResourceCopyTargetType::DEPTH_STENCIL_TARGET, ResourceCopyTargetType::UNORDERED_ACCESS_VIEW})
	{
		fixture.context->ClearState();
		TextureViewFixture input(fixture, kind);
		for (auto mode : {Profiling::Mode::NONE, Profiling::Mode::SUMMARY, Profiling::Mode::TOP_COMMAND_LISTS,
		                  Profiling::Mode::TOP_COMMANDS})
		{
			Profiling::mode = mode;
			Profiling::clear();
			ResourceCopyOperation operation;
			CustomResource source;
			operation.options = ResourceCopyOptions::REFERENCE;
			operation.src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
			operation.src.SetCustomResource(&source);
			operation.dst.type = kind;
			operation.dst.shader_type = kind == ResourceCopyTargetType::UNORDERED_ACCESS_VIEW ? L'c' : L'p';
			operation.dst.SetResource(&fixture.state, input.resources[0].Get(), input.views[0].Get(), 0, 0,
			                          DXGI_FORMAT_UNKNOWN, 0);
			boundary->fail_views = true;
			boundary->view_creations = 0;
			operation.CopyResourceToResource(
			    &fixture.state, input.resources[1].Get(), nullptr, 0, 0,
			    kind == ResourceCopyTargetType::DEPTH_STENCIL_TARGET ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_R32_FLOAT,
			    0);
			auto bound = bound_view(fixture, kind);
			require_view_resource(bound.Get(), input.resources[0].Get());
			require(boundary->view_creations == 1, "required view failure was not exercised");
			require(Profiling::resource_view_creation_overhead.count == (mode == Profiling::Mode::SUMMARY ? 1U : 0U) &&
			            Profiling::resource_view_creation_overhead.hits == 0,
			        "failed view diagnostics changed mode/counts");
			boundary->fail_views = false;
			operation.CopyResourceToResource(
			    &fixture.state, input.resources[1].Get(), nullptr, 0, 0,
			    kind == ResourceCopyTargetType::DEPTH_STENCIL_TARGET ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_R32_FLOAT,
			    0);
			bound = bound_view(fixture, kind);
			require_view_resource(bound.Get(), input.resources[1].Get());
			require(Profiling::resource_view_creation_overhead.count == (mode == Profiling::Mode::SUMMARY ? 2U : 0U) &&
			            Profiling::resource_view_creation_overhead.hits == (mode == Profiling::Mode::SUMMARY ? 1U : 0U),
			        "successful view diagnostics changed mode/counts");
			if (mode != Profiling::Mode::SUMMARY)
				require(Profiling::resource_view_creation_overhead.cpu.QuadPart == 0,
				        "inactive view diagnostics collected CPU time");
			Profiling::clear();
			require(Profiling::resource_view_creation_overhead.count == 0 &&
			            Profiling::resource_view_creation_overhead.hits == 0,
			        "clear retained view diagnostics");
		}
	}
	fixture.require_no_errors();
}

// Mechanism benchmarks use real interpreter/D3D boundaries. They are separate
// from regression cases and keep timings out of pass/fail expectations.

void copies_smaller_constant_buffer_regions()
{
	D3DFixture fixture;
	ConfigFixture resources;
	float values[256]{};
	values[0] = 3;
	values[63] = 7;
	values[64] = 11;
	values[127] = 13;
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(values);
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = values;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, &initial, &buffer)), "small-region source CB failed");
	auto *binding = buffer.Get();
	fixture.context->VSSetConstantBuffers(0, 1, &binding);
	for (bool offset : {false, true})
	{
		CommandList commands;
		parse_copy(commands, L"cs-cb1", offset ? L"copy vs-cb0->region(256,256)" : L"copy vs-cb0->region(0,256)");
		for (const auto &command : commands.commands)
			command->run(&fixture.state);
		Microsoft::WRL::ComPtr<ID3D11Buffer> copied;
		fixture.context->CSGetConstantBuffers(1, 1, &copied);
		require(copied != nullptr, "small CB region did not bind a destination");
		std::vector<float> expected(64, 0.0f);
		expected.front() = offset ? 11.0f : 3.0f;
		expected.back() = offset ? 13.0f : 7.0f;
		require(read_buffer(fixture, copied.Get()) == expected, "small CB region did not copy the requested bytes");
	}
	fixture.require_no_errors();
}

void clears_saved_shader_resource_contents()
{
	D3DFixture fixture;
	ConfigFixture resources;
	customResources[L"resourcesaved"];
	for (UINT writable : {UINT(D3D11_BIND_RENDER_TARGET), UINT(D3D11_BIND_UNORDERED_ACCESS)})
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = desc.Height = 2;
		desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
		desc.Format = DXGI_FORMAT_R32_FLOAT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | writable;
		const float values[] = {2, 3, 5, 7};
		D3D11_SUBRESOURCE_DATA initial{};
		initial.pSysMem = values;
		initial.SysMemPitch = 8;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		require(SUCCEEDED(fixture.device->CreateTexture2D(&desc, &initial, &texture)), "clear source texture failed");
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
		require(SUCCEEDED(fixture.device->CreateShaderResourceView(texture.Get(), nullptr, &srv)),
		        "clear source SRV failed");
		auto *binding = srv.Get();
		fixture.context->PSSetShaderResources(0, 1, &binding);
		CommandList commands;
		parse_copy(commands, L"resourcesaved", L"ref ps-t0");
		for (const auto &command : commands.commands)
			command->run(&fixture.state);
		binding = nullptr;
		fixture.context->PSSetShaderResources(0, 1, &binding);
		ClearViewCommand clear;
		require(clear.target.ParseTarget(L"resourcesaved", true, &config_namespace, nullptr),
		        "saved clear target rejected");
		clear.fval[0] = 9.0f;
		clear.run(&fixture.state);
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
		require(SUCCEEDED(fixture.device->CreateTexture2D(&desc, nullptr, &staging)), "clear staging texture failed");
		fixture.context->CopyResource(staging.Get(), texture.Get());
		D3D11_MAPPED_SUBRESOURCE mapped{};
		require(SUCCEEDED(fixture.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "clear readback failed");
		const auto *row0 = static_cast<const float *>(mapped.pData);
		const auto *row1 = reinterpret_cast<const float *>(static_cast<const char *>(mapped.pData) + mapped.RowPitch);
		const bool correct = row0[0] == 9.0f && row0[1] == 9.0f && row1[0] == 9.0f && row1[1] == 9.0f;
		fixture.context->Unmap(staging.Get(), 0);
		require(correct, "saved SRV prevented clearing the underlying writable resource");
	}
	fixture.require_no_errors();
}

void integer_clears_override_saved_render_target_views()
{
	D3DFixture fixture;
	ConfigFixture resources;
	customResources[L"resourcesaved"];
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = desc.Height = 2;
	desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
	desc.Format = DXGI_FORMAT_R32_UINT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	require(SUCCEEDED(fixture.device->CreateTexture2D(&desc, nullptr, &texture)), "integer clear texture failed");
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
	require(SUCCEEDED(fixture.device->CreateRenderTargetView(texture.Get(), nullptr, &rtv)), "integer RTV failed");
	auto *binding = rtv.Get();
	fixture.context->OMSetRenderTargets(1, &binding, nullptr);
	CommandList commands;
	parse_copy(commands, L"resourcesaved", L"ref o0");
	for (const auto &command : commands.commands)
		command->run(&fixture.state);
	fixture.context->OMSetRenderTargets(0, nullptr, nullptr);
	ClearViewCommand clear;
	require(clear.target.ParseTarget(L"resourcesaved", true, &config_namespace, nullptr),
	        "integer clear target rejected");
	clear.clear_uav_uint = true;
	clear.uval[0] = 0x3f800000;
	clear.fval[0] = 1.0f;
	clear.run(&fixture.state);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
	require(SUCCEEDED(fixture.device->CreateTexture2D(&desc, nullptr, &staging)), "integer clear staging failed");
	fixture.context->CopyResource(staging.Get(), texture.Get());
	D3D11_MAPPED_SUBRESOURCE mapped{};
	require(SUCCEEDED(fixture.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)),
	        "integer clear readback failed");
	const auto *row0 = static_cast<const UINT *>(mapped.pData);
	const auto *row1 = reinterpret_cast<const UINT *>(static_cast<const char *>(mapped.pData) + mapped.RowPitch);
	const bool exact = row0[0] == 0x3f800000 && row0[1] == 0x3f800000 && row1[0] == 0x3f800000 && row1[1] == 0x3f800000;
	fixture.context->Unmap(staging.Get(), 0);
	require(exact, "saved RTV converted an explicit integer clear instead of using the UAV");
	fixture.require_no_errors();
}

void hexadecimal_stencil_clear_keeps_its_explicit_view_kind()
{
	D3DFixture fixture;
	ConfigFixture resources;
	SettingsConfigFixture files;
	std::ofstream(files.config) << "[Logging]\nenabled=0\nshow_warnings=0\n"
	                               "[Include]\nuser_config=runtime-user.ini\n"
	                               "[CommandListStencil]\nclear = od stencil 0xff\n";
	LoadConfigFile();
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = desc.Height = 2;
	desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
	desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	require(SUCCEEDED(fixture.device->CreateTexture2D(&desc, nullptr, &texture)), "stencil source texture failed");
	D3D11_DEPTH_STENCIL_VIEW_DESC view_desc{};
	view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> view;
	require(SUCCEEDED(fixture.device->CreateDepthStencilView(texture.Get(), &view_desc, &view)), "stencil view failed");
	fixture.context->ClearDepthStencilView(view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0x12);
	fixture.context->OMSetRenderTargets(0, nullptr, view.Get());
	RunLinkedCommandList clear(&explicitCommandListSections.at(L"commandliststencil").command_list);
	clear.run(&fixture.state);
	fixture.context->OMSetRenderTargets(0, nullptr, nullptr);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
	require(SUCCEEDED(fixture.device->CreateTexture2D(&desc, nullptr, &staging)), "stencil staging texture failed");
	fixture.context->CopyResource(staging.Get(), texture.Get());
	D3D11_MAPPED_SUBRESOURCE mapped{};
	require(SUCCEEDED(fixture.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "stencil readback failed");
	const auto *row0 = static_cast<const UINT *>(mapped.pData);
	const auto *row1 = reinterpret_cast<const UINT *>(static_cast<const char *>(mapped.pData) + mapped.RowPitch);
	const bool cleared =
	    row0[0] == 0xff000000 && row0[1] == 0xff000000 && row1[0] == 0xff000000 && row1[1] == 0xff000000;
	fixture.context->Unmap(staging.Get(), 0);
	require(cleared, "hexadecimal stencil clear was treated as an integer UAV request or changed depth");
	fixture.require_no_errors();
}

void preserves_learned_pool_flags_during_copy()
{
	D3DFixture fixture;
	ConfigFixture resources;
	auto &pool = make_pool(L"poollearned", 0.0f);
	pool.resource_template->override_type = CustomResourceType::BUFFER;
	pool.resource_template->override_format = DXGI_FORMAT_R32_FLOAT;
	pool.resource_template->override_array = 4;
	ResourceCopyTarget current;
	current.type = ResourceCopyTargetType::SHADER_RESOURCE;
	current.shader_type = L'p';
	fixture.state.this_target = &current;
	CommandList reference;
	parse_copy(reference, L"this", L"ref poollearned[0]");
	for (const auto &command : reference.commands)
		command->run(&fixture.state);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> first;
	fixture.context->PSGetShaderResources(0, 1, &first);
	require(first != nullptr, "pool element failed to learn its initial SRV usage");
	const float values[] = {2, 3, 5, 7};
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(values);
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = values;
	Microsoft::WRL::ComPtr<ID3D11Buffer> source;
	require(SUCCEEDED(fixture.device->CreateBuffer(&desc, &initial, &source)), "pool VB source failed");
	auto *binding = source.Get();
	const UINT stride = 4, offset = 0;
	fixture.context->IASetVertexBuffers(0, 1, &binding, &stride, &offset);
	CommandList copy;
	parse_copy(copy, L"poollearned[0]", L"copy vb0");
	for (const auto &command : copy.commands)
		command->run(&fixture.state);
	ID3D11ShaderResourceView *empty = nullptr;
	fixture.context->PSSetShaderResources(0, 1, &empty);
	for (const auto &command : reference.commands)
		command->run(&fixture.state);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> result;
	fixture.context->PSGetShaderResources(0, 1, &result);
	require(result != nullptr, "pool copy discarded runtime-learned SRV binding support");
	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	result->GetResource(&resource);
	require(read_buffer(fixture, resource.Get()) == std::vector<float>({2, 3, 5, 7}),
	        "pool rebind exposed stale copied values");
	auto &fresh = customResources[L"resourcefreshpoolreference"];
	fresh.override_type = CustomResourceType::BUFFER;
	fresh.override_format = DXGI_FORMAT_R32_FLOAT;
	fresh.override_array = 4;
	const float fresh_values[] = {11, 13, 17, 19};
	fresh.initial_data = malloc(sizeof(fresh_values));
	require(fresh.initial_data != nullptr, "initial reference data allocation failed");
	memcpy(fresh.initial_data, fresh_values, sizeof(fresh_values));
	fresh.initial_data_size = sizeof(fresh_values);
	CommandList fresh_reference;
	parse_copy(fresh_reference, L"poollearned[0]", L"ref resourcefreshpoolreference");
	for (const auto &command : fresh_reference.commands)
		command->run(&fixture.state);
	fixture.context->PSSetShaderResources(0, 1, &empty);
	for (const auto &command : reference.commands)
		command->run(&fixture.state);
	result.Reset();
	fixture.context->PSGetShaderResources(0, 1, &result);
	require(result != nullptr, "initial source creation lost the destination's learned binding requirements");
	resource.Reset();
	result->GetResource(&resource);
	require(resource.Get() == fresh.resource &&
	            read_buffer(fixture, resource.Get()) == std::vector<float>({11, 13, 17, 19}),
	        "newly substantiated pool reference exposed the wrong resource or data");
	customResources[L"resourceemptythenpopulated"].override_format = DXGI_FORMAT_R32_FLOAT;
	CommandList empty_reference;
	parse_copy(empty_reference, L"poollearned[0]", L"ref unless_null resourceemptythenpopulated");
	for (const auto &command : empty_reference.commands)
		command->run(&fixture.state);
	CommandList populate;
	parse_copy(populate, L"resourceemptythenpopulated", L"copy vb0");
	for (const auto &command : populate.commands)
		command->run(&fixture.state);
	CommandList populated_reference;
	parse_copy(populated_reference, L"poollearned[0]", L"ref resourceemptythenpopulated");
	for (const auto &command : populated_reference.commands)
		command->run(&fixture.state);
	fixture.context->PSSetShaderResources(0, 1, &empty);
	for (const auto &command : reference.commands)
		command->run(&fixture.state);
	result.Reset();
	fixture.context->PSGetShaderResources(0, 1, &result);
	require(result != nullptr, "an initially empty reference lost future binding requirements");
	resource.Reset();
	result->GetResource(&resource);
	require(read_buffer(fixture, resource.Get()) == std::vector<float>({2, 3, 5, 7}),
	        "populating a previously empty reference exposed stale data");
	fixture.state.this_target = nullptr;
	fixture.require_no_errors();
}

struct MetadataFixture
{
	D3DFixture d3d;
	ConfigFixture resources;
	ResourceCopyTarget current;

	MetadataFixture()
	{
		current.type = ResourceCopyTargetType::SHADER_RESOURCE;
		current.shader_type = L'p';
		d3d.state.this_target = &current;
	}
	~MetadataFixture()
	{
		d3d.state.this_target = nullptr;
	}

	static void configure_buffer(CustomResource &resource)
	{
		resource.override_type = CustomResourceType::BUFFER;
		resource.override_format = DXGI_FORMAT_R32_FLOAT;
		resource.override_array = 4;
		const float values[] = {11, 13, 17, 19};
		resource.initial_data = malloc(sizeof(values));
		require(resource.initial_data != nullptr, "metadata fixture data allocation failed");
		memcpy(resource.initial_data, values, sizeof(values));
		resource.initial_data_size = sizeof(values);
	}

	void execute(const wchar_t *destination, const wchar_t *source)
	{
		CommandListScope scope;
		CommandList commands;
		commands.scope = &scope;
		parse_copy(commands, destination, source);
		for (const auto &command : commands.commands)
			command->run(&d3d.state);
	}

	void require_binding(CustomResource *expected = nullptr)
	{
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
		d3d.context->PSGetShaderResources(0, 1, &view);
		require(view != nullptr, "metadata preview prevented a later SRV binding");
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		view->GetResource(&resource);
		require(!expected || resource.Get() == expected->resource,
		        "preview and assignment selected different resources");
		require(read_buffer(d3d, resource.Get()) == std::vector<float>({11, 13, 17, 19}),
		        "metadata preview changed the bound values");
	}

	void prepare_outer(const wchar_t *learned)
	{
		auto &outer = make_pool(L"poolouter", 0.0f);
		configure_buffer(*outer.resource_template);
		execute(L"this", learned);
		ID3D11ShaderResourceView *empty = nullptr;
		d3d.context->PSSetShaderResources(0, 1, &empty);
		configure_buffer(customResources[L"resourcepreviewfresh"]);
	}

	void require_fresh_flags()
	{
		auto *resource = customResources[L"resourcepreviewfresh"].resource;
		require(resource != nullptr, "preview source was not created");
		D3D11_BUFFER_DESC desc{};
		static_cast<ID3D11Buffer *>(resource)->GetDesc(&desc);
		require((desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0,
		        "preview selected flags from a different destination than assignment");
	}
};

void metadata_resource_reads_preserve_lazy_templates()
{
	MetadataFixture fixture;
	auto &keys = make_pool(L"poolkeys", 0.0f);
	keys.index_type = PoolIndexType::FIFO;
	keys.Initialize(2);
	MetadataFixture::configure_buffer(*keys.resource_template);
	keys.GetVariable(1, false, false, true)->fval = 1;
	make_pool(L"poolouter", 0.0f);
	customResources[L"resourcepreviewempty"];
	fixture.execute(L"poolouter[@poolkeys[1]]", L"ref unless_null resourcepreviewempty");
	fixture.execute(L"this", L"ref poolkeys[2]");
	fixture.require_binding();
	fixture.d3d.require_no_errors();
}

void metadata_lazy_indexes_match_assignment()
{
	MetadataFixture fixture;
	make_pool(L"poolinner", 0.0f);
	fixture.prepare_outer(L"ref poolouter[0]");
	fixture.execute(L"poolouter[#poolinner[0]]", L"ref resourcepreviewfresh");
	fixture.require_fresh_flags();
	fixture.execute(L"this", L"ref poolouter[0]");
	fixture.require_binding(&customResources[L"resourcepreviewfresh"]);
	fixture.d3d.require_no_errors();
}

void metadata_resource_identities_survive_acceptance()
{
	MetadataFixture fixture;
	Microsoft::WRL::ComPtr<D3DDeviceBoundary> boundary;
	boundary.Attach(new D3DDeviceBoundary(fixture.d3d.device.Get()));
	HackerContext context(boundary.Get(), fixture.d3d.context.Get());
	fixture.d3d.state.mOrigDevice1 = boundary.Get();
	fixture.d3d.state.mHackerContext = &context;
	auto &keys = make_pool(L"poolkeys", 0.0f);
	keys.index_type = PoolIndexType::FIFO;
	keys.Initialize(2);
	MetadataFixture::configure_buffer(*keys.resource_template);
	keys.GetVariable(1, false, false, true)->fval = 1;
	auto &outer = make_pool(L"poolouter", 0.0f);
	outer.index_type = PoolIndexType::FIFO;
	outer.Initialize(2);
	auto &fresh = customResources[L"resourcepreviewfresh"];
	MetadataFixture::configure_buffer(fresh);
	fresh.bind_flags = D3D11_BIND_SHADER_RESOURCE;
	fixture.execute(L"poolouter[@poolkeys[1]]", L"ref resourcepreviewfresh");
	require(boundary->buffer_creations == 2, "accepted preview created a second GPU identity for its nested resource");
	fixture.execute(L"this", L"ref poolouter[@poolkeys[1]]");
	fixture.require_binding(&fresh);
	auto &self = customResources[L"resourcepreviewself"];
	MetadataFixture::configure_buffer(self);
	self.bind_flags = D3D11_BIND_SHADER_RESOURCE;
	fixture.execute(L"poolouter[@resourcepreviewself]", L"ref resourcepreviewself");
	require(boundary->buffer_creations == 3, "self-indexed source creation changed its GPU identity");
	fixture.execute(L"this", L"ref poolouter[@resourcepreviewself]");
	fixture.require_binding(&self);
	fixture.d3d.require_no_errors();
}

void metadata_fifo_reads_match_allocation_order()
{
	MetadataFixture fixture;
	auto &ids = make_pool(L"poolids", 0.0f);
	ids.index_type = PoolIndexType::FIFO;
	ids.allocate_slot_on_missing = true;
	ids.Initialize(2);
	ids.GetVariable(1, false, false, true)->fval = 0;
	ids.GetVariable(2, false, false, true)->fval = 1;
	fixture.prepare_outer(L"ref poolouter[1]");
	fixture.execute(L"poolouter[$poolids[3]+$poolids[4]]", L"ref resourcepreviewfresh");
	fixture.require_fresh_flags();
	fixture.execute(L"this", L"ref poolouter[1]");
	fixture.require_binding(&customResources[L"resourcepreviewfresh"]);
	fixture.d3d.require_no_errors();
}

void metadata_hot_resources_keep_their_identity()
{
	MetadataFixture fixture;
	auto &key = customResources[L"resourcehotkey"];
	MetadataFixture::configure_buffer(key);
	fixture.execute(L"this", L"ref resourcehotkey");
	Microsoft::WRL::ComPtr<ID3D11Resource> original = key.resource;
	require(original != nullptr, "hot key was not created");
	auto &outer = make_pool(L"poolouter", 0.0f);
	outer.index_type = PoolIndexType::FIFO;
	outer.Initialize(2);
	auto &fresh = customResources[L"resourcepreviewfresh"];
	MetadataFixture::configure_buffer(fresh);
	fresh.bind_flags = D3D11_BIND_SHADER_RESOURCE;
	fixture.execute(L"poolouter[@resourcehotkey]", L"ref resourcepreviewfresh");
	require(key.resource == original.Get(), "metadata adoption destroyed an already-created key resource");
	fixture.execute(L"this", L"ref resourcehotkey");
	fixture.require_binding(&key);
	fixture.d3d.require_no_errors();
}

void metadata_spatial_reads_preserve_cached_ranges()
{
	MetadataFixture fixture;
	auto &key = customResources[L"resourcecachedkey"];
	const float coordinates[] = {64, 2, 3};
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(coordinates);
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA initial{};
	initial.pSysMem = coordinates;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	require(SUCCEEDED(fixture.d3d.device->CreateBuffer(&desc, &initial, &buffer)), "cached key buffer failed");
	key.resource = buffer.Detach();
	key.device = fixture.d3d.device.Get();
	key.substantiated = true;
	key.is_null = false;
	key.format = DXGI_FORMAT_R32_FLOAT;
	key.stride = 4;
	key.buf_size = 12;
	ResourceHandleInfo source_cache;
	const float cached[] = {99, 64, 2, 3};
	source_cache.SetDataCache(cached, sizeof(cached));
	key.handle_info = std::make_unique<ResourceHandleInfo>();
	key.handle_info->InitializeDataCache(12, 4);
	key.handle_info->cached_data = source_cache.cached_data;
	CommandListScope scope;
	CommandListExpression query, coarse_query;
	const std::wstring expression = L"resourcecachedkey->spatialhash(0,1,2,1)";
	require(query.parse(&expression, &config_namespace, &scope), "cached spatial query was rejected");
	const std::wstring coarse = L"resourcecachedkey->spatialhash(0,1,2,2)";
	require(coarse_query.parse(&coarse, &config_namespace, &scope), "coarse cached query was rejected");
	require(BitCastToUint(coarse_query.evaluate(&fixture.d3d.state)) == 0x02001001U,
	        "cached spatial fixture has the wrong key");
	auto &outer = make_pool(L"poolouter", 0.0f);
	outer.index_type = PoolIndexType::FIFO;
	MetadataFixture::configure_buffer(*outer.resource_template);
	outer.Initialize(2);
	// Dropping the cache offset reads (99,64,2); that key must have no SRV support.
	outer.GetResource(BitCastToFloat(0x06340002U), false, false, true);
	CustomResource *correct = outer.GetResource(BitCastToFloat(0x04002003U), false, false, true);
	ResourceCopyOperation learn;
	learn.src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
	learn.src.SetCustomResource(correct);
	learn.dst.type = ResourceCopyTargetType::THIS_RESOURCE;
	learn.options = ResourceCopyOptions::REFERENCE;
	learn.run(&fixture.d3d.state);
	MetadataFixture::configure_buffer(customResources[L"resourcepreviewfresh"]);
	fixture.execute(L"poolouter[resourcecachedkey->spatialhash(0,1,2,1)]", L"ref resourcepreviewfresh");
	fixture.require_fresh_flags();
	// Unit cells at (64,2,3), packed into the 12/8/12-bit grid.
	require(BitCastToUint(query.evaluate(&fixture.d3d.state)) == 0x04002003U,
	        "metadata adoption lost cached data ownership or its range");
	fixture.execute(L"this", L"ref poolouter[resourcecachedkey->spatialhash(0,1,2,1)]");
	fixture.require_binding(&customResources[L"resourcepreviewfresh"]);
	fixture.d3d.require_no_errors();
}

void metadata_static_resources_remain_pinned_after_proxy_changes()
{
	MetadataFixture fixture;
	auto &pinned = make_pool(L"poolstaticpin", 0.0f);
	pinned.index_type = PoolIndexType::STATIC;
	auto &other = make_pool(L"poolother", 0.0f);
	fixture.prepare_outer(L"ref poolouter[0]");
	customResourcePools[L"poolouter"].Initialize(4);
	CommandListScope scope;
	CommandList commands;
	commands.scope = &scope;
	parse_copy(commands, L"poolouter[#poolstaticpin[0]]", L"ref resourcepreviewfresh");
	require(pinned.SetSourcePool(&other), "static pool proxy was rejected");
	for (const auto &command : commands.commands)
		command->run(&fixture.d3d.state);
	fixture.require_fresh_flags();
	fixture.execute(L"this", L"ref poolouter[0]");
	fixture.require_binding(&customResources[L"resourcepreviewfresh"]);
	fixture.d3d.require_no_errors();
}

void verify_shared_alias(bool expire_on_dynamic_read, bool reverse_operands = false)
{
	MetadataFixture fixture;
	G->frame_no = 10;
	Microsoft::WRL::ComPtr<D3DDeviceBoundary> boundary;
	boundary.Attach(new D3DDeviceBoundary(fixture.d3d.device.Get()));
	HackerContext context(boundary.Get(), fixture.d3d.context.Get());
	fixture.d3d.state.mOrigDevice1 = boundary.Get();
	fixture.d3d.state.mHackerContext = &context;
	auto &pinned = make_pool(L"poolstaticalias", 0.0f);
	pinned.index_type = PoolIndexType::STATIC;
	MetadataFixture::configure_buffer(*pinned.resource_template);
	if (expire_on_dynamic_read)
	{
		pinned.expiration_timeout_frames = 5;
		pinned.Initialize(2);
		pinned.GetResource(0, false, false, true);
	}
	auto &dynamic = make_pool(L"pooldynamicalias", 0.0f);
	require(dynamic.SetSourcePool(&pinned), "dynamic alias was rejected");
	if (expire_on_dynamic_read)
	{
		auto &outer = make_pool(L"poolouter", 0.0f);
		MetadataFixture::configure_buffer(*outer.resource_template);
		outer.GetResource(0, false, false, true)->bind_flags = D3D11_BIND_SHADER_RESOURCE;
		outer.GetResource(1, false, false, true)->bind_flags = D3D11_BIND_UNORDERED_ACCESS;
		MetadataFixture::configure_buffer(customResources[L"resourcepreviewfresh"]);
	}
	else
		fixture.prepare_outer(L"ref poolouter[1]");
	CommandListScope scope;
	CommandList commands;
	commands.scope = &scope;
	parse_copy(commands,
	           reverse_operands ? L"poolouter[!(@pooldynamicalias[0] - @poolstaticalias[0])]"
	                            : L"poolouter[!(@poolstaticalias[0] - @pooldynamicalias[0])]",
	           L"ref resourcepreviewfresh");
	G->frame_no = expire_on_dynamic_read ? 17 : 10;
	for (const auto &command : commands.commands)
		command->run(&fixture.d3d.state);
	require(boundary->buffer_creations == (expire_on_dynamic_read ? 2U : 3U),
	        "aliases created separate provisional GPU resources for one slot");
	auto &fresh = customResources[L"resourcepreviewfresh"];
	if (expire_on_dynamic_read)
	{
		auto &outer = customResourcePools[L"poolouter"];
		const bool slot0 = outer.GetResource(0, false, false, false)->resource == fresh.resource;
		const bool slot1 = outer.GetResource(1, false, false, false)->resource == fresh.resource;
		require(fresh.resource != nullptr && slot0 != slot1, "expiring alias copy did not write one destination");
		D3D11_BUFFER_DESC desc{};
		static_cast<ID3D11Buffer *>(fresh.resource)->GetDesc(&desc);
		require(desc.BindFlags == static_cast<UINT>(slot0 ? D3D11_BIND_SHADER_RESOURCE : D3D11_BIND_UNORDERED_ACCESS),
		        "expiring alias preview omitted its actual destination's binding capability");
		if (slot1)
		{
			fixture.current.type = ResourceCopyTargetType::UNORDERED_ACCESS_VIEW;
			fixture.current.shader_type = L'c';
			fixture.execute(L"this", L"ref poolouter[1]");
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> view;
			fixture.d3d.context->CSGetUnorderedAccessViews(0, 1, &view);
			require_view_resource(view.Get(), fresh.resource);
			require(read_buffer(fixture.d3d, fresh.resource) == std::vector<float>({11, 13, 17, 19}),
			        "expiring alias UAV exposed the wrong data");
		}
		else
		{
			fixture.execute(L"this", L"ref poolouter[0]");
			fixture.require_binding(&fresh);
		}
	}
	else
	{
		fixture.require_fresh_flags();
		fixture.execute(L"this", L"ref poolouter[1]");
		fixture.require_binding(&fresh);
	}
	fixture.d3d.require_no_errors();
}

void metadata_static_and_dynamic_aliases_share_one_resource()
{
	verify_shared_alias(false);
}

void metadata_aliases_preserve_expiration_order()
{
	for (bool reverse : {false, true})
		verify_shared_alias(true, reverse);
}

void verify_destination_key_after_source_lookup(bool spatial)
{
	MetadataFixture fixture;
	auto &pool = make_pool(L"poolkeycommit", 0.0f);
	pool.index_type = spatial ? PoolIndexType::SPATIAL : PoolIndexType::FIFO;
	pool.spatial_radius = 1;
	pool.allocate_slot_on_missing = !spatial;
	pool.Initialize(1);
	const float source = spatial ? BitCastToFloat(0x04000000U) : 1.0f;
	const float destination = spatial ? BitCastToFloat(0x04100000U) : 2.0f;
	const float probe = spatial ? BitCastToFloat(0x04200000U) : 2.0f;
	command_list_globals.emplace(L"$s", CommandListVariable(L"$s", source, VariableFlags::NONE));
	command_list_globals.emplace(L"$d", CommandListVariable(L"$d", destination, VariableFlags::NONE));
	command_list_globals.emplace(L"$e", CommandListVariable(L"$e", probe, VariableFlags::NONE));
	auto &seed = customResources[L"resourcekeyseed"];
	MetadataFixture::configure_buffer(seed);
	seed.bind_flags = D3D11_BIND_SHADER_RESOURCE;
	fixture.execute(L"poolkeycommit[$s]", L"ref resourcekeyseed");
	fixture.execute(L"this", L"ref poolkeycommit[$s]");
	fixture.require_binding(&seed);
	fixture.execute(L"poolkeycommit[$d]", L"ref poolkeycommit[$s]");
	pool.allocate_slot_on_missing = false;
	fixture.execute(L"this", L"ref poolkeycommit[$e]");
	fixture.require_binding(&seed);
	fixture.d3d.require_no_errors();
}

void spatial_copy_keeps_its_destination_key()
{
	verify_destination_key_after_source_lookup(true);
}

void fifo_copy_keeps_its_destination_key()
{
	verify_destination_key_after_source_lookup(false);
}

void metadata_expiration_matches_same_frame_reset()
{
	MetadataFixture fixture;
	auto &ids = make_pool(L"poolids", 0.0f);
	ids.expiration_timeout_frames = 5;
	ids.Initialize(2);
	G->frame_no = 100;
	ids.GetVariable(0, false, false, true)->fval = 1;
	ids.ResetPool(false);
	fixture.prepare_outer(L"ref poolouter[1]");
	fixture.execute(L"poolouter[$poolids[0]]", L"ref resourcepreviewfresh");
	fixture.require_fresh_flags();
	fixture.execute(L"this", L"ref poolouter[1]");
	fixture.require_binding(&customResources[L"resourcepreviewfresh"]);
	fixture.d3d.require_no_errors();
}

void verify_static_variable_metadata(bool reverse_operands)
{
	MetadataFixture fixture;
	G->frame_no = 10;
	auto &pinned = make_pool(L"poolstaticvars", 0.0f);
	pinned.index_type = PoolIndexType::STATIC;
	pinned.expiration_timeout_frames = 5;
	pinned.Initialize(2);
	pinned.GetVariable(0, false, false, true)->fval = 1.0f;
	auto &dynamic = make_pool(L"pooldynamicvars", 0.0f);
	require(dynamic.SetSourcePool(&pinned), "variable alias was rejected");
	auto &outer = make_pool(L"poolouter", 0.0f);
	MetadataFixture::configure_buffer(*outer.resource_template);
	outer.GetResource(0, false, false, true)->bind_flags = D3D11_BIND_SHADER_RESOURCE;
	outer.GetResource(1, false, false, true)->bind_flags = D3D11_BIND_UNORDERED_ACCESS;
	MetadataFixture::configure_buffer(customResources[L"resourcepreviewfresh"]);
	CommandListScope scope;
	CommandList commands;
	commands.scope = &scope;
	parse_copy(commands,
	           reverse_operands ? L"poolouter[$poolstaticvars[0] + $pooldynamicvars[0]]"
	                            : L"poolouter[$pooldynamicvars[0] + $poolstaticvars[0]]",
	           L"ref resourcepreviewfresh");
	G->frame_no = 17;
	for (const auto &command : commands.commands)
		command->run(&fixture.d3d.state);
	auto *fresh = customResources[L"resourcepreviewfresh"].resource;
	const bool slot0 = outer.GetResource(0, false, false, false)->resource == fresh;
	const bool slot1 = outer.GetResource(1, false, false, false)->resource == fresh;
	require(fresh != nullptr && slot0 != slot1, "ordinary variable reads did not select one destination");
	D3D11_BUFFER_DESC desc{};
	static_cast<ID3D11Buffer *>(fresh)->GetDesc(&desc);
	require(desc.BindFlags == static_cast<UINT>(slot0 ? D3D11_BIND_SHADER_RESOURCE : D3D11_BIND_UNORDERED_ACCESS),
	        "STATIC variable metadata read stale live state instead of the shared expired preview");
	fixture.d3d.require_no_errors();
}

void static_variable_metadata_matches_expiring_dynamic_aliases()
{
	for (bool reverse : {false, true})
		verify_static_variable_metadata(reverse);
	MetadataFixture fixture;
	auto &pinned = make_pool(L"poolpinnedvars", 0.0f);
	pinned.index_type = PoolIndexType::STATIC;
	pinned.GetVariable(0, false, false, true)->fval = 1.0f;
	auto &other = make_pool(L"poolothervars", 0.0f);
	auto &outer = make_pool(L"poolouter", 0.0f);
	MetadataFixture::configure_buffer(*outer.resource_template);
	outer.GetResource(0, false, false, true)->bind_flags = D3D11_BIND_UNORDERED_ACCESS;
	outer.GetResource(1, false, false, true)->bind_flags = D3D11_BIND_SHADER_RESOURCE;
	MetadataFixture::configure_buffer(customResources[L"resourcepreviewfresh"]);
	CommandListScope scope;
	CommandList commands;
	commands.scope = &scope;
	parse_copy(commands, L"poolouter[$poolpinnedvars[0]]", L"ref resourcepreviewfresh");
	require(pinned.SetSourcePool(&other), "STATIC variable proxy change rejected");
	for (const auto &command : commands.commands)
		command->run(&fixture.d3d.state);
	fixture.execute(L"this", L"ref poolouter[1]");
	fixture.require_binding(&customResources[L"resourcepreviewfresh"]);
	fixture.d3d.require_no_errors();
}

void scalar_variables_keep_precedence_over_pool_names()
{
	ConfigFixture resources;
	make_pool(L"poolfoo", 2.0f);
	command_list_globals.emplace(L"$poolfoo", CommandListVariable(L"$poolfoo", 7.0f, VariableFlags::NONE));
	CommandListVariable local(L"$poolfoo", 11.0f, VariableFlags::NONE);
	CommandListScope scope;
	scope.emplace_front();
	scope.front().emplace(L"$poolfoo", &local);
	CommandListState state;
	CommandListExpression local_expression, global_expression, indexed_expression;
	const std::wstring plain = L"$poolfoo", indexed = L"$poolfoo[0]";
	require(local_expression.parse(&plain, &config_namespace, &scope), "local pool-prefixed scalar was rejected");
	require(local_expression.evaluate(&state) == 11.0f, "pool name shadowed a local scalar");
	scope.clear();
	require(global_expression.parse(&plain, &config_namespace, &scope), "global pool-prefixed scalar was rejected");
	require(global_expression.evaluate(&state) == 7.0f, "pool name shadowed a global scalar");
	require(indexed_expression.parse(&indexed, &config_namespace, &scope), "indexed pool variable was rejected");
	require(indexed_expression.evaluate(&state) == 2.0f, "indexed pool variable lost precedence");
}

void this_references_reuse_views_until_the_kind_changes()
{
	D3DFixture fixture;
	BufferViewFixture input(fixture);
	CustomResource source;
	Microsoft::WRL::ComPtr<D3DDeviceBoundary> boundary;
	boundary.Attach(new D3DDeviceBoundary(fixture.device.Get()));
	fixture.state.mOrigDevice1 = boundary.Get();
	ResourceCopyTarget current;
	current.type = ResourceCopyTargetType::SHADER_RESOURCE;
	current.shader_type = L'p';
	fixture.state.this_target = &current;
	ResourceCopyOperation operation;
	operation.options = ResourceCopyOptions::REFERENCE;
	operation.src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
	operation.src.SetCustomResource(&source);
	operation.dst.type = ResourceCopyTargetType::THIS_RESOURCE;
	for (int i = 0; i < 3; ++i)
		operation.CopyResourceToResource(&fixture.state, input.buffer.Get(), nullptr, 4, 0, DXGI_FORMAT_R32_FLOAT, 32);
	require_buffer_view(fixture, input.buffer.Get(), DXGI_FORMAT_R32_FLOAT, 0, 8);
	require(boundary->view_creations == 1, "stable this references recreated a matching view");
	fixture.context->ClearState();
	current.type = ResourceCopyTargetType::UNORDERED_ACCESS_VIEW;
	current.shader_type = L'c';
	operation.CopyResourceToResource(&fixture.state, input.buffer.Get(), nullptr, 4, 0, DXGI_FORMAT_R32_FLOAT, 32);
	auto bound = bound_view(fixture, ResourceCopyTargetType::UNORDERED_ACCESS_VIEW);
	require_view_resource(bound.Get(), input.buffer.Get());
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
	require(SUCCEEDED(bound.As(&uav)), "this reference retained the prior wrong-kind view");
	require(boundary->view_creations == 2, "this kind change did not create exactly one required replacement view");
	fixture.state.this_target = nullptr;
	fixture.require_no_errors();
}

void zero_frame_updates_preserve_reload_measurements()
{
	D3DFixture fixture;
	ConfigFixture resources;
	SettingsConfigFixture files;
	ProfilingFixture profiling;
	std::ofstream(files.config) << "[Constants]\nglobal $initialized = 0\n$initialized = 7\n";
	for (auto mode : {Profiling::Mode::SUMMARY, Profiling::Mode::TOP_COMMAND_LISTS, Profiling::Mode::TOP_COMMANDS})
	{
		Profiling::mode = mode;
		LoadConfigFile();
		RunLinkedCommandList initialization(&G->constants_command_list);
		initialization.run(&fixture.state);
		require(command_list_globals.at(L"$initialized").fval == 7.0f, "reload initialization did not execute");
		Profiling::resource_full_copies = 3;
		Profiling::update_txt();
		require(Profiling::resource_full_copies == 3, "zero-frame overlay update discarded resource measurements");
		if (mode == Profiling::Mode::TOP_COMMANDS)
		{
			require(command_lists_cmd_profiling.size() == 1,
			        "zero-frame overlay update discarded initialized command registrations");
			require(G->constants_command_list.commands.at(0)->pre_executions == 1,
			        "zero-frame overlay update discarded active command measurements");
		}
		else
			require(G->constants_command_list.executions == 1,
			        "zero-frame overlay update discarded active list measurements");
	}
}

struct TextureQueryFixture
{
	D3DFixture d3d;
	std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 2> buffers;
	std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2> views;
	std::vector<DirectX::XMFLOAT4> saved_rows = G->iniParams;
	CommandListExpression query, repeated, assigned;

	explicit TextureQueryFixture(bool validation = true) : d3d(validation)
	{
		G->iniParams.resize(50);
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = 32;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SHADER_RESOURCE_VIEW_DESC view{};
		view.Format = DXGI_FORMAT_R32_FLOAT;
		view.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		view.Buffer.NumElements = 8;
		for (size_t index = 0; index < buffers.size(); ++index)
		{
			require(SUCCEEDED(d3d.device->CreateBuffer(&desc, nullptr, &buffers[index])), "query buffer failed");
			require(SUCCEEDED(d3d.device->CreateShaderResourceView(buffers[index].Get(), &view, &views[index])),
			        "query view failed");
			const uint32_t hash = index == 0 ? 0x1234U : 0x5678U;
			G->mResources[buffers[index].Get()].hash = hash;
			auto &rule = G->mTextureOverrideMap[hash].emplace_back();
			rule.filter_index = index == 0 ? 158.0f : 215.0f;
		}
		const std::wstring read = L"ps-t0", repeat = L"ps-t0 == 158", reuse = L"w49 == 158";
		require(query.parse(&read, &config_namespace, nullptr) && repeated.parse(&repeat, &config_namespace, nullptr) &&
		            assigned.parse(&reuse, &config_namespace, nullptr),
		        "query expressions were rejected");
		bind(0);
	}

	void bind(size_t index)
	{
		auto *view = views[index].Get();
		d3d.context->PSSetShaderResources(0, 1, &view);
	}

	~TextureQueryFixture()
	{
		G->iniParams = std::move(saved_rows);
		for (auto &buffer : buffers)
			G->mResources.erase(buffer.Get());
		G->mTextureOverrideMap.erase(0x1234);
		G->mTextureOverrideMap.erase(0x5678);
	}
};

void repeated_queries_and_assigned_rows_follow_rebinding()
{
	TextureQueryFixture fixture;
	for (const size_t index : {0U, 1U, 0U})
	{
		fixture.bind(index);
		for (int repeat = 0; repeat < 3; ++repeat)
		{
			G->iniParams[49].w = fixture.query.evaluate(&fixture.d3d.state);
			require(G->iniParams[49].w == (index == 0 ? 158.0f : 215.0f), "query retained a previous binding");
			require(fixture.repeated.evaluate(&fixture.d3d.state) == (index == 0 ? 1.0f : 0.0f),
			        "repeated query changed the filter predicate");
			require(fixture.assigned.evaluate(&fixture.d3d.state) == (index == 0 ? 1.0f : 0.0f),
			        "assigned row changed the filter predicate");
		}
	}
	fixture.d3d.require_no_errors();
}

void benchmark_texture_queries(int samples)
{
	TextureQueryFixture fixture(false);
	ProfilingFixture profiling;
	DWORD_PTR process_mask = 0, system_mask = 0;
	require(GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask) != 0 && process_mask != 0,
	        "query benchmark affinity unavailable");
	const DWORD_PTR selected_cpu = process_mask & (~process_mask + 1);
	require(SetThreadAffinityMask(GetCurrentThread(), selected_cpu) != 0, "query benchmark affinity failed");
	std::cerr << "ENV query_benchmark_thread_affinity=" << selected_cpu << '\n';
	constexpr int iterations = 100000;
	std::cout << "workload,mode,sample,variant,iterations,ns_per_pair,row_value,predicate_sum,match_calls\n";
	for (auto mode : {Profiling::Mode::NONE, Profiling::Mode::SUMMARY})
	{
		Profiling::mode = mode;
		for (bool rebind : {false, true})
		{
			for (int sample = 0; sample < samples; ++sample)
			{
				// Alternate the within-process order; report every paired sample.
				for (bool assigned : {sample % 2 != 0, sample % 2 == 0})
				{
					fixture.bind(0);
					Profiling::clear();
					float predicates = 0;
					auto &expression = assigned ? fixture.assigned : fixture.repeated;
					const auto start = std::chrono::steady_clock::now();
					for (int iteration = 0; iteration < iterations; ++iteration)
					{
						if (rebind)
							fixture.bind(static_cast<size_t>(iteration) % 2);
						G->iniParams[49].w = fixture.query.evaluate(&fixture.d3d.state);
						predicates += expression.evaluate(&fixture.d3d.state);
					}
					const auto elapsed = std::chrono::steady_clock::now() - start;
					require(predicates == (rebind ? 50000.0f : 100000.0f), "query benchmark predicate changed");
					require(G->iniParams[49].w == (rebind ? 215.0f : 158.0f), "query benchmark final row changed");
					std::cout << (rebind ? "rebind" : "stable") << ',' << static_cast<int>(mode) << ',' << sample << ','
					          << (assigned ? "assigned_row" : "repeated_query") << ',' << iterations << ','
					          << std::chrono::duration<double, std::nano>(elapsed).count() / iterations << ','
					          << G->iniParams[49].w << ',' << predicates << ','
					          << Profiling::texture_override_match_overhead.count << '\n';
				}
			}
		}
	}
}

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
			std::cout << static_cast<int>(selected_mode) << ',' << sample << ',' << iterations << ','
			          << std::chrono::duration<double, std::nano>(elapsed).count() / iterations << ','
			          << command_lists_profiling.size() << ',' << command_lists_cmd_profiling.size() << ','
			          << value.fval << '\n';
		}
	}
	Profiling::mode = Profiling::Mode::NONE;
	Profiling::clear();
}

void benchmark_command_entries(int samples)
{
	IniParamsFixture fixture(false);
	DWORD_PTR process_mask = 0, system_mask = 0;
	require(GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask) != FALSE && process_mask != 0,
	        "entry benchmark process affinity is unavailable");
	const DWORD_PTR selected_cpu = process_mask & (~process_mask + 1);
	require(SetThreadAffinityMask(GetCurrentThread(), selected_cpu) != 0, "entry benchmark thread affinity failed");
	std::cerr << "ENV entry_benchmark_thread_affinity=" << selected_cpu << '\n';
	auto inserted = command_list_globals.emplace(L"$entrybenchmark",
	                                             CommandListVariable(L"$entrybenchmark", 0.0f, VariableFlags::NONE));
	auto &value = inserted.first->second;
	std::array<CommandList, 64> empty, generated, proxies, nonempty;
	for (std::size_t index = 0; index < empty.size(); ++index)
	{
		generated[index].runtime_populated = true;
		require(proxies[index].SetSourceCommandList(&empty[index]), "benchmark proxy was rejected");
		auto assignment = std::make_shared<VariableAssignment>();
		assignment->var = &value;
		const std::wstring expression = L"$entrybenchmark + 1";
		require(assignment->expression.parse(&expression, &config_namespace, nullptr),
		        "entry benchmark expression rejected");
		nonempty[index].commands.push_back(assignment);
	}
	constexpr int iterations = 200000;
	std::cout << "workload,mode,sample,iterations,ns_per_run,value\n";
	for (auto mode : {Profiling::Mode::NONE, Profiling::Mode::SUMMARY})
	{
		Profiling::mode = mode;
		for (int workload = 0; workload < 4; ++workload)
		{
			auto *lists = workload == 0 ? &empty : workload == 1 ? &proxies : workload == 2 ? &generated : &nonempty;
			const char *name = workload == 0   ? "empty"
			                   : workload == 1 ? "proxy_empty"
			                   : workload == 2 ? "dynamic_empty"
			                                   : "one_assignment";
			for (int sample = 0; sample < samples; ++sample)
			{
				Profiling::clear();
				value.fval = 0;
				const auto start = std::chrono::steady_clock::now();
				for (int iteration = 0; iteration < iterations; ++iteration)
					RunCommandList(&fixture.device, fixture.d3d.hacker_context.get(),
					               &(*lists)[static_cast<std::size_t>(iteration) % lists->size()], nullptr, false);
				const double elapsed =
				    std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
				require(value.fval == (workload == 3 ? 200000.0f : 0.0f), "entry benchmark changed command results");
				std::cout << name << ',' << static_cast<int>(mode) << ',' << sample << ',' << iterations << ','
				          << elapsed / iterations << ',' << value.fval << '\n';
			}
		}
	}
	Profiling::mode = Profiling::Mode::NONE;
	Profiling::clear();
	command_list_globals.erase(L"$entrybenchmark");
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
	CustomResource source;
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
				operation.src.SetCustomResource(&source);
				operation.dst.type = ResourceCopyTargetType::SHADER_RESOURCE;
				operation.dst.shader_type = L'p';
				operation.dst.slot = 0;
				boundary->view_creations = 0;
				Profiling::clear();
				const auto begin = std::chrono::steady_clock::now();
				for (int i = 0; i < iterations; ++i)
				{
					const size_t index = static_cast<size_t>(i) % resources;
					operation.CopyResourceToResource(&fixture.state, buffers[index].Get(), views[index].Get(), 4, 0,
					                                 DXGI_FORMAT_R32_FLOAT, 64);
				}
				const auto elapsed = std::chrono::steady_clock::now() - begin;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bound;
				fixture.context->PSGetShaderResources(0, 1, &bound);
				Microsoft::WRL::ComPtr<ID3D11Resource> bound_resource;
				if (bound)
					bound->GetResource(&bound_resource);
				const auto last = static_cast<size_t>(iterations - 1) % resources;
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
	G->gShowWarnings = false;
	InitializeCriticalSection(&G->mCriticalSection);
	InitializeCriticalSection(&G->mResourcesLock);
	InitializeCriticalSection(&resource_creation_mode_lock);
	if (argc > 1 && (std::string(argv[1]) == "benchmark-lists" || std::string(argv[1]) == "benchmark-views" ||
	                 std::string(argv[1]) == "benchmark-entries" || std::string(argv[1]) == "benchmark-queries"))
	{
		const int samples = argc > 2 ? std::stoi(argv[2]) : 11;
		if (samples < 1 || samples > 1000)
			return 1;
		if (std::string(argv[1]) == "benchmark-lists")
			benchmark_command_lists(samples);
		else if (std::string(argv[1]) == "benchmark-entries")
			benchmark_command_entries(samples);
		else if (std::string(argv[1]) == "benchmark-queries")
			benchmark_texture_queries(samples);
		else
			benchmark_resource_views(samples);
		return 0;
	}
	const bool startup_benchmark = argc > 1 && std::string(argv[1]) == "benchmark-startup";
	if (argc > 1 && (std::string(argv[1]) == "benchmark-load" || startup_benchmark))
	{
		const int runs = argc > 2 ? std::stoi(argv[2]) : 21;
		if (runs < 1 || runs > 1000)
			return 1;
		std::cout << "sample,load_us," << (startup_benchmark ? "optimize_us," : "") << "globals,constants_commands\n";
		for (int sample = 0; sample < runs; ++sample)
		{
			ClearKeyBindings();
			G->iniParamsReserved = 0;
			const auto start = std::chrono::steady_clock::now();
			LoadConfigFile();
			const auto elapsed = std::chrono::steady_clock::now() - start;
			std::cout << sample << ',' << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() << ',';
			if (startup_benchmark)
			{
				const auto optimize_start = std::chrono::steady_clock::now();
				optimise_command_lists(nullptr);
				const auto optimize_elapsed = std::chrono::steady_clock::now() - optimize_start;
				std::cout << std::chrono::duration_cast<std::chrono::microseconds>(optimize_elapsed).count() << ',';
			}
			else
			{
				// Complete the normal load lifecycle so optimizer-owned lists do
				// not accumulate across parse-only timing samples.
				optimise_command_lists(nullptr);
			}
			std::cout << command_list_globals.size() << ',' << G->constants_command_list.commands.size() << '\n';
		}
		return 0;
	}
	const struct
	{
		const char *name;
		void (*run)();
	} tests[] = {{"empty-groups", rejects_empty_groups},
	             {"leading-dot", accepts_leading_dot_numbers},
	             {"pending-save", preserves_pending_save},
	             {"pool-reset", resets_only_the_requested_pool_proxy},
	             {"constant-stride", recreates_structured_data_as_a_constant_buffer},
	             {"iniparams-transition-retry", final_transition_upload_retries_after_retirement},
	             {"iniparams-command-retry", command_upload_retries_after_its_state_retires},
	             {"iniparams-contexts", deferred_recording_does_not_acknowledge_immediate_publication},
	             {"entry-path", empty_entry_paths_preserve_later_work_and_profiling},
	             {"query-assigned-row", repeated_queries_and_assigned_rows_follow_rebinding},
	             {"reference-order", resolves_reference_chains_in_any_order},
	             {"reference-cycles", terminates_incompatible_reference_cycles},
	             {"pool-metadata", metadata_queries_do_not_extend_pool_lifetime},
	             {"missing-metadata", missing_resource_metadata_has_no_bind_flags},
	             {"nested-metadata", skipped_copies_do_not_evaluate_nested_destination_indexes},
	             {"metadata-resource", metadata_resource_reads_preserve_lazy_templates},
	             {"metadata-resource-identity", metadata_resource_identities_survive_acceptance},
	             {"metadata-lazy-index", metadata_lazy_indexes_match_assignment},
	             {"metadata-fifo", metadata_fifo_reads_match_allocation_order},
	             {"metadata-expiration", metadata_expiration_matches_same_frame_reset},
	             {"metadata-hot-identity", metadata_hot_resources_keep_their_identity},
	             {"metadata-cached-spatial", metadata_spatial_reads_preserve_cached_ranges},
	             {"metadata-static-pin", metadata_static_resources_remain_pinned_after_proxy_changes},
	             {"metadata-alias", metadata_static_and_dynamic_aliases_share_one_resource},
	             {"metadata-alias-expiration", metadata_aliases_preserve_expiration_order},
	             {"metadata-destination-key-spatial", spatial_copy_keeps_its_destination_key},
	             {"metadata-destination-key-fifo", fifo_copy_keeps_its_destination_key},
	             {"pool-identifiers", accepts_pool_identifier_grammar},
	             {"store-values", stores_typed_and_structured_values},
	             {"store-views", releases_store_views_on_success_and_failure},
	             {"store-failure", preserves_store_values_when_readback_allocation_fails},
	             {"region-copy", copies_region_byte_counts_and_preserves_invalid_destinations},
	             {"region-vertex-bytes", vertex_regions_use_byte_counts_instead_of_binding_stride},
	             {"region-views", region_views_follow_changing_ranges},
	             {"region-cb", copies_cb_regions_beyond_the_bound_window},
	             {"settings-atomic", failed_replacement_preserves_complete_file},
	             {"settings-lifecycle", preserves_only_eligible_settings_across_reload},
	             {"settings-deferred", deferred_initialization_preserves_unloaded_settings},
	             {"settings-save-update", saves_acknowledge_only_the_captured_settings},
	             {"settings-batch-reload", reload_defers_until_deferred_command_batches_finish},
	             {"settings-save-reload", reload_waits_for_an_inflight_autosave},
	             {"analysis-reload", frame_analysis_input_keeps_configuration_ownership},
	             {"constants-order", preserves_constants_declaration_and_execution_order},
	             {"ini-sections", preserves_duplicate_sections_and_recursive_includes},
	             {"if-conditions", preserves_static_and_dynamic_conditions_across_reload},
	             {"expression-grammar", preserves_expression_operator_contract},
	             {"profiling-collections", profiling_collections_follow_mode_and_window_changes},
	             {"profiling-reload", profiling_reload_discards_old_command_collections},
	             {"matching-diagnostics", matching_diagnostics_preserve_hash_first_and_order},
	             {"view-reuse", reuses_matching_views_with_owned_lifetimes},
	             {"view-descriptors", incompatible_views_use_correct_factory_descriptors},
	             {"view-cache-metadata", cached_views_follow_changing_source_metadata},
	             {"view-ownership", custom_references_retain_only_destination_ownership},
	             {"view-failure", failed_view_creation_preserves_bindings_and_selected_metrics},
	             {"region-cb-small", copies_smaller_constant_buffer_regions},
	             {"view-clear", clears_saved_shader_resource_contents},
	             {"view-clear-integer", integer_clears_override_saved_render_target_views},
	             {"view-clear-stencil", hexadecimal_stencil_clear_keeps_its_explicit_view_kind},
	             {"metadata-static-variable", static_variable_metadata_matches_expiring_dynamic_aliases},
	             {"pool-learned-flags", preserves_learned_pool_flags_during_copy},
	             {"pool-scalar-precedence", scalar_variables_keep_precedence_over_pool_names},
	             {"view-this-cache", this_references_reuse_views_until_the_kind_changes},
	             {"profiling-zero-frame", zero_frame_updates_preserve_reload_measurements},
	             {"resource-final-release", final_resource_release_removes_live_metadata},
	             {"resource-retirement", resource_release_survives_registry_destruction}};
	int failures = 0;
	int selected = 0;
	for (const auto &test : tests)
	{
		if (argc > 1 && std::string(argv[1]) != test.name)
			continue;
		try
		{
			++selected;
			std::cout << "RUN " << test.name << '\n';
			test.run();
			std::cout << "PASS " << test.name << '\n';
		}
		catch (const std::exception &error)
		{
			std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
			++failures;
		}
	}
	return failures || selected == 0 ? 1 : 0;
}
