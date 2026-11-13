#include "../DirectX11/CommandList.h"
#include "../DirectX11/HackerContext.h"
#include "../DirectX11/IniHandler.h"
#include "../DirectX11/globals.h"
#include "D3DDeviceBoundary.h"
#include "../DirectX11/input.h"
#include "../DirectX11/profiling.h"

#include <atomic>
#include <chrono>
#include <clocale>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <d3d11sdklayers.h>
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
		std::cout << "[DEBUG-view-descriptor] example=" << example << "\n";
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
	if (argc > 1 && (std::string(argv[1]) == "benchmark-lists" || std::string(argv[1]) == "benchmark-views"))
	{
		const int samples = argc > 2 ? std::stoi(argv[2]) : 11;
		if (samples < 1 || samples > 1000)
			return 1;
		if (std::string(argv[1]) == "benchmark-lists")
			benchmark_command_lists(samples);
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
	             {"reference-order", resolves_reference_chains_in_any_order},
	             {"reference-cycles", terminates_incompatible_reference_cycles},
	             {"pool-metadata", metadata_queries_do_not_extend_pool_lifetime},
	             {"pool-identifiers", accepts_pool_identifier_grammar},
	             {"store-values", stores_typed_and_structured_values},
	             {"store-views", releases_store_views_on_success_and_failure},
	             {"store-failure", preserves_store_values_when_readback_allocation_fails},
	             {"region-copy", copies_region_byte_counts_and_preserves_invalid_destinations},
	             {"region-views", region_views_follow_changing_ranges},
	             {"region-cb", copies_cb_regions_beyond_the_bound_window},
	             {"settings-atomic", failed_replacement_preserves_complete_file},
	             {"settings-lifecycle", preserves_only_eligible_settings_across_reload},
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
	             {"view-failure", failed_view_creation_preserves_bindings_and_selected_metrics}};
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
