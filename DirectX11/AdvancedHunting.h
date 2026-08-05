#pragma once

#include <d3d11_1.h>
#include <stdint.h>

#include "DrawCallInfo.h"

class HackerDevice;

enum class AdvancedHuntingShaderStage {
	NONE,
	VERTEX,
	PIXEL,
};

enum class AdvancedHuntingScope {
	AUTO,
	RESOURCE,
	DRAW,
};

struct AdvancedHuntingContext
{
	AdvancedHuntingShaderStage shader_stage;
	UINT64 shader_hash;
	DrawCall draw_type;
	D3D11_PRIMITIVE_TOPOLOGY topology;
	UINT vertex_count;
	UINT index_count;
	UINT instance_count;
	UINT first_vertex;
	UINT first_index;
	UINT first_instance;
	uint32_t index_buffer;
	uint32_t vertex_buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	uint32_t pixel_shader_resources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	uint32_t render_targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	uint32_t depth_target;
	uint32_t indirect_buffer;
	UINT indirect_args_offset;

	AdvancedHuntingContext();
};

struct AdvancedHuntingOverlayInfo
{
	bool active;
	bool capture_locked;
	bool verbose;
	bool selected;
	bool context_limit_reached;
	AdvancedHuntingShaderStage shader_stage;
	AdvancedHuntingScope scope;
	UINT64 shader_hash;
	size_t context_position;
	size_t context_count;
	unsigned matches_this_frame;
	unsigned observed_frames;
	unsigned observations;
	uint64_t fingerprint;
	AdvancedHuntingContext context;

	AdvancedHuntingOverlayInfo();
};

bool AdvancedHuntingConfigured();
bool AdvancedHuntingActive();
bool GetAdvancedHuntingStateForDraw(UINT64 vertex_shader, UINT64 pixel_shader,
	AdvancedHuntingShaderStage *stage, UINT64 *hash, bool *parent_matches);
bool SubmitAdvancedHuntingContext(const AdvancedHuntingContext &context);
void ResetAdvancedHunting();
void ParseAdvancedHuntingSection(int repeat);
bool GetAdvancedHuntingOverlayInfo(AdvancedHuntingOverlayInfo *info);

const wchar_t *AdvancedHuntingScopeName(AdvancedHuntingScope scope);
const wchar_t *AdvancedHuntingStageName(AdvancedHuntingShaderStage stage);
const wchar_t *AdvancedHuntingDrawTypeName(DrawCall type);
