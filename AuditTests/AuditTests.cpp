#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "D3D_Shaders/DxbcContainer.h"
#include "DirectX11/ComOutput.h"

static unsigned failures;

static void Check(bool condition, const char *message)
{
	if (condition)
		return;
	std::fprintf(stderr, "FAIL: %s\n", message);
	++failures;
}

static void WriteUint32(std::vector<uint8_t> *data, size_t offset, uint32_t value)
{
	std::memcpy(data->data() + offset, &value, sizeof(value));
}

static std::vector<uint8_t> MakeDxbc(const char tag[4] = "SHEX")
{
	std::vector<uint8_t> data(52, 0);
	std::memcpy(data.data(), "DXBC", 4);
	WriteUint32(&data, 24, static_cast<uint32_t>(data.size()));
	WriteUint32(&data, 28, 1);
	WriteUint32(&data, 32, 36);
	std::memcpy(data.data() + 36, tag, 4);
	WriteUint32(&data, 40, 8);
	WriteUint32(&data, 44, 0x00000050);
	WriteUint32(&data, 48, 2);
	return data;
}

static void TestDxbcValidation()
{
	DxbcCodeChunkInfo info = {};
	std::vector<uint8_t> valid = MakeDxbc();
	Check(FindDxbcCodeChunk(valid.data(), valid.size(), &info), "valid DXBC rejected");
	Check(info.num_chunks == 1 && info.code_chunk_index == 0, "valid DXBC metadata is incorrect");
	Check(info.code_chunk_offset == 36 && info.code_chunk_size == 8, "valid code chunk bounds are incorrect");
	Check(info.code_chunk == valid.data() + 36, "valid code chunk pointer is incorrect");

	for (size_t size = 0; size < valid.size(); ++size)
		Check(!FindDxbcCodeChunk(valid.data(), size, &info), "truncated DXBC accepted");

	std::vector<uint8_t> mutated = valid;
	mutated[0] = 'X';
	Check(!FindDxbcCodeChunk(mutated.data(), mutated.size(), &info), "bad DXBC signature accepted");

	mutated = valid;
	WriteUint32(&mutated, 28, 0xffffffffu);
	Check(!FindDxbcCodeChunk(mutated.data(), mutated.size(), &info), "overflowing chunk table accepted");

	mutated = valid;
	WriteUint32(&mutated, 32, 32);
	Check(!FindDxbcCodeChunk(mutated.data(), mutated.size(), &info), "overlapping chunk accepted");

	mutated.assign(56, 0);
	std::memcpy(mutated.data(), "DXBC", 4);
	WriteUint32(&mutated, 24, static_cast<uint32_t>(mutated.size()));
	WriteUint32(&mutated, 28, 1);
	WriteUint32(&mutated, 32, 37);
	std::memcpy(mutated.data() + 37, "SHEX", 4);
	WriteUint32(&mutated, 41, 8);
	Check(!FindDxbcCodeChunk(mutated.data(), mutated.size(), &info), "unaligned code chunk accepted");

	mutated = valid;
	WriteUint32(&mutated, 40, 4);
	Check(!FindDxbcCodeChunk(mutated.data(), mutated.size(), &info), "undersized code chunk accepted");

	mutated = valid;
	WriteUint32(&mutated, 40, 9);
	Check(!FindDxbcCodeChunk(mutated.data(), mutated.size(), &info), "unaligned code payload accepted");

	mutated = MakeDxbc("RDEF");
	Check(!FindDxbcCodeChunk(mutated.data(), mutated.size(), &info), "container without shader code accepted");

	mutated.assign(72, 0);
	std::memcpy(mutated.data(), "DXBC", 4);
	WriteUint32(&mutated, 24, static_cast<uint32_t>(mutated.size()));
	WriteUint32(&mutated, 28, 2);
	WriteUint32(&mutated, 32, 40);
	WriteUint32(&mutated, 36, 56);
	std::memcpy(mutated.data() + 40, "SHDR", 4);
	WriteUint32(&mutated, 44, 8);
	std::memcpy(mutated.data() + 56, "SHEX", 4);
	WriteUint32(&mutated, 60, 8);
	Check(!FindDxbcCodeChunk(mutated.data(), mutated.size(), &info), "multiple code chunks accepted");

	uint32_t random = 0x13579bdfu;
	for (size_t iteration = 0; iteration < 10000; ++iteration) {
		random = random * 1664525u + 1013904223u;
		std::vector<uint8_t> fuzz((random >> 24) + 1);
		for (size_t i = 0; i < fuzz.size(); ++i) {
			random = random * 1664525u + 1013904223u;
			fuzz[i] = static_cast<uint8_t>(random >> 24);
		}
		FindDxbcCodeChunk(fuzz.data(), fuzz.size(), &info);
	}
}

struct FakeComObject
{
	unsigned releases = 0;
	unsigned long Release() noexcept
	{
		return ++releases;
	}
};

static void TestComFailureOutput()
{
	FakeComObject object;
	FakeComObject *output = &object;
	HRESULT result = NormalizeComFailureOutput(E_FAIL, &output);
	Check(result == E_FAIL, "COM failure result changed");
	Check(output == nullptr, "COM failure output was not cleared");
	Check(object.releases == 1, "COM failure output was not released exactly once");

	output = &object;
	result = NormalizeComFailureOutput(S_OK, &output);
	Check(result == S_OK && output == &object, "COM success output was changed");
	Check(object.releases == 1, "COM success output was released");

	output = nullptr;
	Check(NormalizeComFailureOutput(E_FAIL, &output) == E_FAIL, "null COM failure output changed result");
	Check(NormalizeComFailureOutput<FakeComObject>(E_FAIL, nullptr) == E_FAIL, "null COM output address changed result");
}

int main()
{
	TestDxbcValidation();
	TestComFailureOutput();
	if (failures) {
		std::fprintf(stderr, "%u audit contract test(s) failed\n", failures);
		return 1;
	}
	std::puts("Audit contract tests passed");
	return 0;
}
