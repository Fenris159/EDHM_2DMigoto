#pragma once

#include <d3d11_1.h>
#include <cstddef>
#include <stdint.h>
#include <string>
#include <vector>

#include "DrawCallInfo.h"

class HackerDevice;

enum class AdvancedHuntingShaderStage
{
	NONE,
	VERTEX,
	PIXEL,
};

enum class AdvancedHuntingScope
{
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
	bool deferred_context_seen;
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

class AdvancedHuntingState
{
  private:
	struct ContextEntry
	{
		AdvancedHuntingContext context;
		unsigned last_seen_frame{};
		unsigned observed_frames{};
		unsigned observations{};
	};

	bool active_{};
	bool capture_locked_{};
	bool verbose_{};
	bool selected_{};
	bool context_limit_reached_{};
	bool deferred_context_seen_{};
	AdvancedHuntingShaderStage shader_stage_{AdvancedHuntingShaderStage::NONE};
	AdvancedHuntingScope scope_{AdvancedHuntingScope::AUTO};
	AdvancedHuntingScope default_scope_{AdvancedHuntingScope::AUTO};
	UINT64 shader_hash_{};
	size_t max_contexts_{512};
	unsigned lifetime_frames_{120};
	unsigned current_frame_{};
	unsigned selected_match_frame_{};
	unsigned selected_matches_this_frame_{};
	AdvancedHuntingContext selected_context_;
	std::vector<ContextEntry> contexts_;

	bool ContextLess(const ContextEntry &lhs, const ContextEntry &rhs) const;
	bool ContextEquals(const AdvancedHuntingContext &lhs, const AdvancedHuntingContext &rhs) const;
	std::vector<ContextEntry>::iterator FindContext(const AdvancedHuntingContext &context);
	std::vector<ContextEntry>::const_iterator FindContext(const AdvancedHuntingContext &context) const;
	void PruneStaleContexts();

  public:
	void Configure(AdvancedHuntingScope default_scope, bool verbose, size_t max_contexts, unsigned lifetime_frames);
	void Reset();
	void Enter(AdvancedHuntingShaderStage stage, UINT64 hash, unsigned frame_no);
	bool Active() const;
	bool GetStateForDraw(UINT64 vertex_shader, UINT64 pixel_shader, AdvancedHuntingShaderStage *stage, UINT64 *hash,
	                     bool *parent_matches) const;
	void AdvanceFrame(unsigned frame_no);
	bool Submit(const AdvancedHuntingContext &context);
	bool Select(bool next);
	bool ChangeScope(bool next, AdvancedHuntingScope *scope);
	bool ToggleCapture(bool *locked);
	void NoteDeferredContext();
	bool GetOverlayInfo(AdvancedHuntingOverlayInfo *info) const;
};

bool AdvancedHuntingConfigured();
bool AdvancedHuntingActive();
bool GetAdvancedHuntingStateForDraw(UINT64 vertex_shader, UINT64 pixel_shader, AdvancedHuntingShaderStage *stage,
                                    UINT64 *hash, bool *parent_matches);
bool SubmitAdvancedHuntingContext(const AdvancedHuntingContext &context);
bool AdvancedHuntingSupportsContextType(D3D11_DEVICE_CONTEXT_TYPE type);
void NoteAdvancedHuntingDeferredContext();
void AdvanceAdvancedHuntingFrame();
void ResetAdvancedHunting();
void ParseAdvancedHuntingSection(int repeat);
bool GetAdvancedHuntingOverlayInfo(AdvancedHuntingOverlayInfo *info);

int CompareAdvancedHuntingContexts(const AdvancedHuntingContext &lhs, const AdvancedHuntingContext &rhs,
                                   AdvancedHuntingScope scope);
uint64_t FingerprintAdvancedHuntingContext(const AdvancedHuntingContext &context, AdvancedHuntingScope scope);
std::string BuildAdvancedHuntingContextReport(const AdvancedHuntingOverlayInfo &info);

const wchar_t *AdvancedHuntingScopeName(AdvancedHuntingScope scope);
const wchar_t *AdvancedHuntingStageName(AdvancedHuntingShaderStage stage);
const wchar_t *AdvancedHuntingDrawTypeName(DrawCall type);
