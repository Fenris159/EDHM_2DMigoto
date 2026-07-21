#pragma once

#include <cstddef>
#include <cstdint>

struct DxbcCodeChunkInfo
{
	uint32_t num_chunks;
	uint32_t code_chunk_index;
	uint32_t code_chunk_offset;
	uint32_t code_chunk_size;
	const uint8_t *code_chunk;
};

// Validates a DXBC container and locates its single SHEX/SHDR code chunk.
// The returned pointer remains owned by the caller's input buffer.
bool FindDxbcCodeChunk(const void *data, size_t size, DxbcCodeChunkInfo *info);
