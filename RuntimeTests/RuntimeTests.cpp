#include "../DirectX11/CommandList.h"
#include "../DirectX11/IniHandler.h"
#include "../DirectX11/globals.h"

#include <clocale>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <wrl/client.h>

namespace
{
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
	source.variable_template = std::make_unique<CommandListVariable>(L"$source", 9.0f, VariableFlags::NONE);
	proxy.variable_template = std::make_unique<CommandListVariable>(L"$proxy", 3.0f, VariableFlags::NONE);
	unrelated.variable_template = std::make_unique<CommandListVariable>(L"$other", 5.0f, VariableFlags::NONE);
	source.Initialize(2);
	proxy.Initialize(2);
	unrelated.Initialize(2);
	require(proxy.SetSourcePool(&source), "could not establish pool proxy");
	CommandList commands;
	std::wstring value = L"null";
	require(ParseCommandListResourceCopyTargetDirective(L"CommandListTest", L"poolproxy", &value, &commands, nullptr),
	        "pool proxy reset was rejected");
	CommandListState state;
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
	CommandListState state;

	D3DFixture()
	{
		require(module != nullptr, "could not load system D3D11");
		auto address = GetProcAddress(module, "D3D11CreateDevice");
		decltype(&D3D11CreateDevice) create = nullptr;
		static_assert(sizeof(create) == sizeof(address), "unexpected function pointer size");
		memcpy(&create, &address, sizeof(create));
		require(create != nullptr, "system D3D11CreateDevice is unavailable");
		Microsoft::WRL::ComPtr<ID3D11Device> base_device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> base_context;
		require(SUCCEEDED(create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &base_device,
		                         nullptr, &base_context)),
		        "could not create WARP device");
		require(SUCCEEDED(base_device.As(&device)), "D3D11.1 device unavailable");
		require(SUCCEEDED(base_context.As(&context)), "D3D11.1 context unavailable");
		state.mOrigDevice1 = device.Get();
		state.mOrigContext1 = context.Get();
	}

	~D3DFixture()
	{
		if (context)
			context->ClearState();
		context.Reset();
		device.Reset();
		if (module)
			FreeLibrary(module);
	}
};

void recreates_structured_data_as_a_constant_buffer()
{
	D3DFixture fixture;
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
}

int main(int argc, char **argv)
{
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	G->gShowWarnings = false;
	InitializeCriticalSection(&G->mCriticalSection);
	InitializeCriticalSection(&G->mResourcesLock);
	InitializeCriticalSection(&resource_creation_mode_lock);
	const struct
	{
		const char *name;
		void (*run)();
	} tests[] = {{"empty-groups", rejects_empty_groups},
	             {"leading-dot", accepts_leading_dot_numbers},
	             {"pending-save", preserves_pending_save},
	             {"pool-reset", resets_only_the_requested_pool_proxy},
	             {"constant-stride", recreates_structured_data_as_a_constant_buffer}};
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
