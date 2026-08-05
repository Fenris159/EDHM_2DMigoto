#include "DxbcContainer.h"

#include <cstring>

static bool ReadUint32(const uint8_t *data, size_t size, size_t offset, uint32_t *value)
{
	if (!value || offset > size || sizeof(*value) > size - offset)
		return false;
	std::memcpy(value, data + offset, sizeof(*value));
	return true;
}

bool FindDxbcCodeChunk(const void *data, size_t size, DxbcCodeChunkInfo *info,
		bool allow_empty_code_chunk)
{
	if (info)
		*info = {};
	if (!data || !info || size < 32)
		return false;

	const uint8_t *base = static_cast<const uint8_t*>(data);
	if (std::memcmp(base, "DXBC", 4))
		return false;

	uint32_t file_size;
	uint32_t num_chunks;
	if (!ReadUint32(base, size, 24, &file_size) ||
			!ReadUint32(base, size, 28, &num_chunks) ||
			!num_chunks || file_size < 32 || file_size > size)
		return false;

	uint64_t chunk_table_end = 32 + static_cast<uint64_t>(num_chunks) * sizeof(uint32_t);
	if (chunk_table_end > file_size)
		return false;

	uint32_t found_code_chunks = 0;
	DxbcCodeChunkInfo candidate = {};
	for (uint32_t i = 0; i < num_chunks; ++i) {
		uint32_t chunk_offset;
		uint32_t chunk_size;
		if (!ReadUint32(base, file_size, 32 + static_cast<size_t>(i) * sizeof(uint32_t), &chunk_offset) ||
				chunk_offset < chunk_table_end ||
				static_cast<uint64_t>(chunk_offset) + 8 > file_size ||
				!ReadUint32(base, file_size, static_cast<size_t>(chunk_offset) + 4, &chunk_size) ||
				static_cast<uint64_t>(chunk_offset) + 8 + chunk_size > file_size)
			return false;

		const uint8_t *chunk = base + chunk_offset;
		if (std::memcmp(chunk, "SHEX", 4) && std::memcmp(chunk, "SHDR", 4))
			continue;
		// SignatureParser manufactures an empty SHEX placeholder that the
		// assembler immediately replaces. Only that internal path may opt in to
		// accepting an empty code chunk; external DXBC remains strictly checked.
		bool valid_code_size = chunk_size >= 8 || (allow_empty_code_chunk && !chunk_size);
		if ((chunk_offset & 3) || (chunk_size & 3) || !valid_code_size ||
				++found_code_chunks != 1)
			return false;

		candidate.num_chunks = num_chunks;
		candidate.code_chunk_index = i;
		candidate.code_chunk_offset = chunk_offset;
		candidate.code_chunk_size = chunk_size;
		candidate.code_chunk = chunk;
	}

	if (found_code_chunks != 1)
		return false;
	*info = candidate;
	return true;
}
