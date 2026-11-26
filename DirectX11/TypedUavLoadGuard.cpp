#include "TypedUavLoadGuard.h"

#include <cstring>

namespace
{
constexpr uint32_t dxbc = 0x43425844;
constexpr uint32_t sfi0 = 0x30494653;
constexpr uint32_t shdr = 0x52444853;
constexpr uint32_t shex = 0x58454853;
constexpr uint64_t additionalFormats = 0x800;
constexpr uint32_t extended = 0x80000000;

// memcpy also accepts unaligned bytecode without violating aliasing rules.
uint32_t word(const unsigned char *bytes, size_t index) noexcept
{
	uint32_t value;
	std::memcpy(&value, bytes + index * sizeof(value), sizeof(value));
	return value;
}

bool skip_extensions(const unsigned char *code, size_t &at, size_t end, uint32_t token) noexcept
{
	while (token & extended)
	{
		if (at == end)
			return false;
		token = word(code, at++);
	}
	return true;
}

// Walk operand boundaries, including immediate constants and relative indexes.
// Looking for UAV-like words anywhere in an instruction could match literal data.
bool operand(const unsigned char *code, size_t &at, size_t end, unsigned depth = 0) noexcept
{
	if (at == end || depth > 8)
		return false;
	const uint32_t token = word(code, at++);
	if (!skip_extensions(code, at, end, token))
		return false;
	const unsigned dimensions = (token >> 20) & 3;
	for (unsigned index = 0; index < dimensions; ++index)
	{
		const unsigned representation = (token >> (22 + index * 3)) & 7;
		if (representation > 4)
			return false;
		const size_t immediate = representation == 0 || representation == 3   ? 1
		                         : representation == 1 || representation == 4 ? 2
		                                                                      : 0;
		if (immediate > end - at)
			return false;
		at += immediate;
		if (representation >= 2 && !operand(code, at, end, depth + 1))
			return false;
	}
	const unsigned type = (token >> 12) & 0xff;
	if (type == 4 || type == 5) // immediate32 / immediate64
	{
		const unsigned components = token & 3;
		if (components != 1 && components != 2)
			return false;
		const size_t count = (components == 1 ? size_t{1} : size_t{4}) * (type == 5 ? size_t{2} : size_t{1});
		if (count > end - at)
			return false;
		at += count;
	}
	return true;
}

uint64_t load_slots(const unsigned char *code, size_t at, size_t end, unsigned opcode) noexcept
{
	// ld_uav_typed: dst, address, UAV. The feedback variant adds a status dst.
	const unsigned preceding = opcode == 225 ? 3 : 2;
	for (unsigned index = 0; index < preceding; ++index)
		if (!operand(code, at, end))
			return ~uint64_t{0};
	if (at == end)
		return ~uint64_t{0};
	const uint32_t token = word(code, at++);
	if (!skip_extensions(code, at, end, token))
		return ~uint64_t{0};
	if (((token >> 12) & 0xff) != 30 || ((token >> 20) & 3) != 1 || ((token >> 22) & 7) != 0 || at == end)
		return ~uint64_t{0};
	const uint32_t slot = word(code, at);
	return slot < 64 ? uint64_t{1} << slot : ~uint64_t{0};
}

bool inspect_code(const unsigned char *code, size_t size, uint64_t &mask) noexcept
{
	if (size < 8 || size % 4 || word(code, 1) != size / 4)
		return false;
	const size_t words = size / 4;
	for (size_t at = 2; at < words;)
	{
		const uint32_t token = word(code, at);
		const unsigned opcode = token & 0x7ff;
		size_t count = (token >> 24) & 0x7f;
		if (opcode == 53) // customdata stores its length in the next word
		{
			if (words - at < 2)
				return false;
			count = word(code, at + 1);
			if (count < 2)
				return false;
		}
		if (!count || count > words - at)
			return false;
		const size_t end = at + count;
		if (opcode == 163 || opcode == 225)
		{
			size_t operands = at + 1;
			if (!skip_extensions(code, operands, end, token))
				return false;
			mask |= load_slots(code, operands, end, opcode);
		}
		at = end;
	}
	return true;
}
} // namespace

TypedUavLoadInfo InspectTypedUavLoads(const void *bytecode, size_t length) noexcept
{
	if (!bytecode || length < 32)
		return {};
	const auto *bytes = static_cast<const unsigned char *>(bytecode);
	if (word(bytes, 0) != dxbc || word(bytes, 5) != 1)
		return {};
	const size_t total = word(bytes, 6);
	const size_t chunks = word(bytes, 7);
	if (total < 32 || total > length || chunks > (total - 32) / 4)
		return {};
	const size_t tableEnd = 32 + chunks * 4;
	TypedUavLoadInfo result;
	for (size_t index = 0; index < chunks; ++index)
	{
		const size_t offset = word(bytes, 8 + index);
		if (offset < tableEnd || offset > total || total - offset < 8)
			return {};
		const auto *chunk = bytes + offset;
		const uint32_t type = word(chunk, 0);
		const size_t size = word(chunk, 1);
		if (size > total - offset - 8)
			return {};
		if (type == sfi0)
		{
			if (size < 8)
				return {};
			result.requiresAdditionalFormats |= (word(chunk, 2) & additionalFormats) != 0;
		}
		else if ((type == shex || type == shdr) && !inspect_code(chunk + 8, size, result.typedLoadUavMask))
			return {};
	}
	return result;
}

bool IsAlwaysSupportedTypedUavLoadFormat(DXGI_FORMAT format) noexcept
{
	return format == DXGI_FORMAT_R32_FLOAT || format == DXGI_FORMAT_R32_UINT || format == DXGI_FORMAT_R32_SINT;
}

bool IsConcreteTypedUav(const D3D11_UNORDERED_ACCESS_VIEW_DESC &desc) noexcept
{
	return desc.Format != DXGI_FORMAT_UNKNOWN &&
	       !(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER && (desc.Buffer.Flags & D3D11_BUFFER_UAV_FLAG_RAW));
}
