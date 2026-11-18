#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace
{
void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

std::vector<char> read(const std::filesystem::path &path)
{
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	require(stream.good(), "cannot read shader file");
	const auto length = stream.tellg();
	require(length > 0 && length <= 16 * 1024 * 1024, "shader must contain 1 byte to 16 MiB");
	std::vector<char> bytes(static_cast<size_t>(length));
	stream.seekg(0);
	stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	require(stream.good(), "shader read was incomplete");
	return bytes;
}

HRESULT create_shader(ID3D11Device *device, const std::wstring &stage, const void *data, size_t size)
{
	if (stage == L"ps")
	{
		ComPtr<ID3D11PixelShader> shader;
		const HRESULT result = device->CreatePixelShader(data, size, nullptr, &shader);
		require(FAILED(result) || shader.Get() != nullptr, "pixel creation succeeded without an object");
		return result;
	}
	require(stage == L"vs", "shader stage must be ps or vs");
	ComPtr<ID3D11VertexShader> shader;
	const HRESULT result = device->CreateVertexShader(data, size, nullptr, &shader);
	require(FAILED(result) || shader.Get() != nullptr, "vertex creation succeeded without an object");
	return result;
}

void self_test(ID3D11Device *device)
{
	const char *pixel = "float4 main() : SV_Target { return float4(1,0,0,1); }";
	const char *vertex = "float4 main(uint id : SV_VertexID) : SV_Position { return float4(id,0,0,1); }";
	for (const auto &stage : {std::wstring(L"ps"), std::wstring(L"vs")})
	{
		ComPtr<ID3DBlob> code, errors;
		const char *source = stage == L"ps" ? pixel : vertex;
		require(SUCCEEDED(D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, "main",
		                             stage == L"ps" ? "ps_5_0" : "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &code,
		                             &errors)),
		        "authored fixture compilation failed");
		require(SUCCEEDED(create_shader(device, stage, code->GetBufferPointer(), code->GetBufferSize())),
		        "authored shader creation failed");
		const std::array<char, 16> malformed{'D', 'X', 'B', 'C'};
		require(FAILED(create_shader(device, stage, malformed.data(), malformed.size())),
		        "malformed shader was accepted");
	}
	std::cout << "PASS prepared shader creation and malformed-bytecode rejection (PS/VS)\n";
}
}

int wmain(int argc, wchar_t **argv)
{
	if (argc < 2 || (std::wstring(argv[1]) != L"--self-test" && argc % 2 != 1))
	{
		std::cerr << "Usage: PreparedShaderTests.exe --self-test | (ps|vs) file.dxbc [...]\n";
		return 2;
	}
	HMODULE module = LoadLibraryExW(L"d3d11.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
	try
	{
		require(module != nullptr, "system D3D11 unavailable");
		auto address = GetProcAddress(module, "D3D11CreateDevice");
		decltype(&D3D11CreateDevice) create = nullptr;
		static_assert(sizeof(create) == sizeof(address), "unexpected pointer size");
		std::memcpy(&create, &address, sizeof(create));
		require(create != nullptr, "system D3D11CreateDevice unavailable");
		ComPtr<ID3D11Device> device;
		const D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_11_0;
		D3D_FEATURE_LEVEL selected{};
		require(SUCCEEDED(create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &requested, 1, D3D11_SDK_VERSION, &device,
		                         &selected, nullptr)),
		        "WARP device creation failed");
		std::cout << "ENV driver_requested=WARP feature_level=" << std::hex << static_cast<unsigned>(selected)
		          << std::dec << " scope=shader-object-creation-only\n";
		bool passed = true;
		if (std::wstring(argv[1]) == L"--self-test")
		{
			require(argc == 2, "self-test accepts no paths");
			self_test(device.Get());
		}
		else
		{
			for (int index = 1; index < argc; index += 2)
			{
				const std::filesystem::path path(argv[index + 1]);
				const auto bytes = read(path);
				const HRESULT result = create_shader(device.Get(), argv[index], bytes.data(), bytes.size());
				require(read(path) == bytes, "input changed during shader validation");
				std::wcout << (SUCCEEDED(result) ? L"PASS " : L"FAIL ") << argv[index] << L" HRESULT=0x" << std::hex
				           << static_cast<unsigned long>(result) << std::dec << L" bytes=" << bytes.size() << L" file="
				           << path.wstring() << L'\n';
				passed &= SUCCEEDED(result);
			}
		}
		device.Reset();
		FreeLibrary(module);
		return passed ? 0 : 1;
	}
	catch (const std::exception &error)
	{
		std::cerr << "FAIL prepared shader check: " << error.what() << '\n';
		if (module)
			FreeLibrary(module);
		return 1;
	}
}
