#include "AdvancedHunting.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "Globals.h"
#include "Hunting.h"
#include "IniHandler.h"
#include "Input.h"
#include "Overlay.h"
#include "log.h"

struct AdvancedHuntingContextEntry
{
	AdvancedHuntingContext context;
	unsigned last_seen_frame;
	unsigned observed_frames;
	unsigned observations;
};

static struct AdvancedHuntingState
{
	bool active;
	bool capture_locked;
	bool verbose;
	bool selected;
	bool context_limit_reached;
	AdvancedHuntingShaderStage shader_stage;
	AdvancedHuntingScope scope;
	AdvancedHuntingScope default_scope;
	UINT64 shader_hash;
	size_t max_contexts;
	unsigned lifetime_frames;
	unsigned last_capture_frame;
	unsigned selected_match_frame;
	unsigned selected_matches_this_frame;
	AdvancedHuntingContext selected_context;
	std::vector<AdvancedHuntingContextEntry> contexts;

	AdvancedHuntingState()
	    : active(false), capture_locked(false), verbose(false), selected(false), context_limit_reached(false),
	      shader_stage(AdvancedHuntingShaderStage::NONE), scope(AdvancedHuntingScope::AUTO),
	      default_scope(AdvancedHuntingScope::AUTO), shader_hash(0), max_contexts(512), lifetime_frames(120),
	      last_capture_frame(0), selected_match_frame(0), selected_matches_this_frame(0)
	{
	}
} advanced_hunting;

static bool ContextLess(const AdvancedHuntingContextEntry &lhs, const AdvancedHuntingContextEntry &rhs)
{
	return CompareAdvancedHuntingContexts(lhs.context, rhs.context, advanced_hunting.scope) < 0;
}

static bool ContextEquals(const AdvancedHuntingContext &lhs, const AdvancedHuntingContext &rhs)
{
	return CompareAdvancedHuntingContexts(lhs, rhs, advanced_hunting.scope) == 0;
}

static std::vector<AdvancedHuntingContextEntry>::iterator FindContext(const AdvancedHuntingContext &context)
{
	AdvancedHuntingContextEntry key;
	key.context = context;

	std::vector<AdvancedHuntingContextEntry>::iterator it =
	    std::lower_bound(advanced_hunting.contexts.begin(), advanced_hunting.contexts.end(), key, ContextLess);
	if (it != advanced_hunting.contexts.end() && ContextEquals(it->context, context))
		return it;
	return advanced_hunting.contexts.end();
}

const wchar_t *AdvancedHuntingScopeName(AdvancedHuntingScope scope)
{
	switch (scope)
	{
	case AdvancedHuntingScope::AUTO:
		return L"AUTO";
	case AdvancedHuntingScope::RESOURCE:
		return L"RESOURCE";
	case AdvancedHuntingScope::DRAW:
		return L"DRAW";
	}
	return L"UNKNOWN";
}

const wchar_t *AdvancedHuntingStageName(AdvancedHuntingShaderStage stage)
{
	switch (stage)
	{
	case AdvancedHuntingShaderStage::VERTEX:
		return L"VS";
	case AdvancedHuntingShaderStage::PIXEL:
		return L"PS";
	default:
		return L"NONE";
	}
}

const wchar_t *AdvancedHuntingDrawTypeName(DrawCall type)
{
	switch (type)
	{
	case DrawCall::Draw:
		return L"Draw";
	case DrawCall::DrawIndexed:
		return L"DrawIndexed";
	case DrawCall::DrawInstanced:
		return L"DrawInstanced";
	case DrawCall::DrawIndexedInstanced:
		return L"DrawIndexedInstanced";
	case DrawCall::DrawInstancedIndirect:
		return L"DrawInstancedIndirect";
	case DrawCall::DrawIndexedInstancedIndirect:
		return L"DrawIndexedInstancedIndirect";
	case DrawCall::DrawAuto:
		return L"DrawAuto";
	default:
		return L"Unknown";
	}
}

bool AdvancedHuntingConfigured()
{
	return G->advanced_hunting_enabled;
}

bool AdvancedHuntingActive()
{
	bool active;
	EnterCriticalSectionPretty(&G->mCriticalSection);
	active = advanced_hunting.active;
	LeaveCriticalSection(&G->mCriticalSection);
	return active;
}

bool GetAdvancedHuntingStateForDraw(UINT64 vertex_shader, UINT64 pixel_shader, AdvancedHuntingShaderStage *stage,
                                    UINT64 *hash, bool *parent_matches)
{
	bool active = false;
	bool matches = false;

	EnterCriticalSectionPretty(&G->mCriticalSection);
	if (advanced_hunting.active)
	{
		active = true;
		if (advanced_hunting.shader_stage == AdvancedHuntingShaderStage::PIXEL)
			matches = pixel_shader == advanced_hunting.shader_hash;
		else if (advanced_hunting.shader_stage == AdvancedHuntingShaderStage::VERTEX)
			matches = vertex_shader == advanced_hunting.shader_hash;
		if (stage)
			*stage = advanced_hunting.shader_stage;
		if (hash)
			*hash = advanced_hunting.shader_hash;
	}
	LeaveCriticalSection(&G->mCriticalSection);

	if (parent_matches)
		*parent_matches = matches;
	return active;
}

static void PruneStaleContexts(unsigned frame_no)
{
	if (advanced_hunting.capture_locked || !advanced_hunting.lifetime_frames)
		return;

	advanced_hunting.contexts.erase(
	    std::remove_if(advanced_hunting.contexts.begin(), advanced_hunting.contexts.end(),
		               [frame_no](const AdvancedHuntingContextEntry &entry)
		               { return frame_no - entry.last_seen_frame > advanced_hunting.lifetime_frames; }),
	    advanced_hunting.contexts.end());

	if (advanced_hunting.selected && FindContext(advanced_hunting.selected_context) == advanced_hunting.contexts.end())
		advanced_hunting.selected = false;

	if (advanced_hunting.contexts.size() < advanced_hunting.max_contexts)
		advanced_hunting.context_limit_reached = false;
}

bool SubmitAdvancedHuntingContext(const AdvancedHuntingContext &context)
{
	bool selected = false;
	unsigned frame_no = G->frame_no;

	EnterCriticalSectionPretty(&G->mCriticalSection);
	if (!advanced_hunting.active || context.shader_stage != advanced_hunting.shader_stage ||
	    context.shader_hash != advanced_hunting.shader_hash)
	{
		LeaveCriticalSection(&G->mCriticalSection);
		return false;
	}

	if (advanced_hunting.last_capture_frame != frame_no)
	{
		advanced_hunting.last_capture_frame = frame_no;
		PruneStaleContexts(frame_no);
	}

	std::vector<AdvancedHuntingContextEntry>::iterator it = FindContext(context);
	if (it != advanced_hunting.contexts.end())
	{
		it->observations++;
		if (it->last_seen_frame != frame_no)
		{
			it->last_seen_frame = frame_no;
			it->observed_frames++;
		}
	}
	else if (!advanced_hunting.capture_locked)
	{
		if (advanced_hunting.contexts.size() >= advanced_hunting.max_contexts)
		{
			advanced_hunting.context_limit_reached = true;
		}
		else
		{
			AdvancedHuntingContextEntry entry;
			entry.context = context;
			entry.last_seen_frame = frame_no;
			entry.observed_frames = 1;
			entry.observations = 1;

			std::vector<AdvancedHuntingContextEntry>::iterator insert_at = std::lower_bound(
			    advanced_hunting.contexts.begin(), advanced_hunting.contexts.end(), entry, ContextLess);
			advanced_hunting.contexts.insert(insert_at, entry);
		}
	}

	selected = advanced_hunting.selected && ContextEquals(context, advanced_hunting.selected_context);
	if (selected)
	{
		if (advanced_hunting.selected_match_frame != frame_no)
		{
			advanced_hunting.selected_match_frame = frame_no;
			advanced_hunting.selected_matches_this_frame = 0;
		}
		advanced_hunting.selected_matches_this_frame++;
	}

	LeaveCriticalSection(&G->mCriticalSection);
	return selected;
}

void ResetAdvancedHunting()
{
	EnterCriticalSectionPretty(&G->mCriticalSection);
	advanced_hunting.active = false;
	advanced_hunting.capture_locked = false;
	advanced_hunting.selected = false;
	advanced_hunting.context_limit_reached = false;
	advanced_hunting.shader_stage = AdvancedHuntingShaderStage::NONE;
	advanced_hunting.shader_hash = 0;
	advanced_hunting.last_capture_frame = 0;
	advanced_hunting.selected_match_frame = 0;
	advanced_hunting.selected_matches_this_frame = 0;
	advanced_hunting.contexts.clear();
	LeaveCriticalSection(&G->mCriticalSection);
}

bool GetAdvancedHuntingOverlayInfo(AdvancedHuntingOverlayInfo *info)
{
	if (!info)
		return false;

	EnterCriticalSectionPretty(&G->mCriticalSection);
	if (!advanced_hunting.active)
	{
		LeaveCriticalSection(&G->mCriticalSection);
		return false;
	}

	info->active = true;
	info->capture_locked = advanced_hunting.capture_locked;
	info->verbose = advanced_hunting.verbose;
	info->selected = advanced_hunting.selected;
	info->context_limit_reached = advanced_hunting.context_limit_reached;
	info->shader_stage = advanced_hunting.shader_stage;
	info->scope = advanced_hunting.scope;
	info->shader_hash = advanced_hunting.shader_hash;
	info->context_count = advanced_hunting.contexts.size();
	info->matches_this_frame =
	    advanced_hunting.selected_match_frame == G->frame_no ? advanced_hunting.selected_matches_this_frame : 0;

	if (advanced_hunting.selected)
	{
		std::vector<AdvancedHuntingContextEntry>::iterator it = FindContext(advanced_hunting.selected_context);
		if (it != advanced_hunting.contexts.end())
		{
			info->context_position = std::distance(advanced_hunting.contexts.begin(), it) + 1;
			info->observed_frames = it->observed_frames;
			info->observations = it->observations;
		}
		else
		{
			info->context_position = 0;
		}
		info->context = advanced_hunting.selected_context;
		info->fingerprint =
		    FingerprintAdvancedHuntingContext(advanced_hunting.selected_context, advanced_hunting.scope);
	}

	LeaveCriticalSection(&G->mCriticalSection);
	return true;
}

static void ToggleAdvancedHunting(HackerDevice *, void *)
{
	AdvancedHuntingShaderStage stage = AdvancedHuntingShaderStage::NONE;
	UINT64 hash = 0;
	if (G->hunting != HUNTING_MODE_ENABLED)
	{
		LogOverlay(LOG_NOTICE, "> Enable shader hunting before entering context hunting\n");
		return;
	}

	EnterCriticalSectionPretty(&G->mCriticalSection);
	if (advanced_hunting.active)
	{
		LeaveCriticalSection(&G->mCriticalSection);
		ResetAdvancedHunting();
		LogOverlay(LOG_INFO, "> Returned to shader-level hunting\n");
		return;
	}

	if (G->mSelectedPixelShader && G->mSelectedPixelShader != (UINT64)-1)
	{
		stage = AdvancedHuntingShaderStage::PIXEL;
		hash = G->mSelectedPixelShader;
	}
	else if (G->mSelectedVertexShader && G->mSelectedVertexShader != (UINT64)-1)
	{
		stage = AdvancedHuntingShaderStage::VERTEX;
		hash = G->mSelectedVertexShader;
	}

	if (stage == AdvancedHuntingShaderStage::NONE)
	{
		LeaveCriticalSection(&G->mCriticalSection);
		LogOverlay(LOG_NOTICE, "> Select a pixel or vertex shader before entering context hunting\n");
		return;
	}

	advanced_hunting.active = true;
	advanced_hunting.capture_locked = false;
	advanced_hunting.selected = false;
	advanced_hunting.context_limit_reached = false;
	advanced_hunting.shader_stage = stage;
	advanced_hunting.shader_hash = hash;
	advanced_hunting.scope = advanced_hunting.default_scope;
	advanced_hunting.last_capture_frame = 0;
	advanced_hunting.selected_match_frame = 0;
	advanced_hunting.selected_matches_this_frame = 0;
	advanced_hunting.contexts.clear();
	LeaveCriticalSection(&G->mCriticalSection);

	LogOverlay(LOG_INFO, "> Context hunting enabled for %S %016I64x\n", AdvancedHuntingStageName(stage), hash);
}

static void SelectContext(bool next)
{
	EnterCriticalSectionPretty(&G->mCriticalSection);
	if (!advanced_hunting.active || advanced_hunting.contexts.empty())
	{
		LeaveCriticalSection(&G->mCriticalSection);
		LogOverlay(LOG_NOTICE, "> No draw contexts have been observed for the selected shader\n");
		return;
	}

	size_t pos;
	if (!advanced_hunting.selected)
	{
		pos = next ? 0 : advanced_hunting.contexts.size() - 1;
	}
	else
	{
		std::vector<AdvancedHuntingContextEntry>::iterator it = FindContext(advanced_hunting.selected_context);
		if (it == advanced_hunting.contexts.end())
		{
			pos = next ? 0 : advanced_hunting.contexts.size() - 1;
		}
		else
		{
			pos = std::distance(advanced_hunting.contexts.begin(), it);
			if (next)
				pos = (pos + 1) % advanced_hunting.contexts.size();
			else
				pos = pos ? pos - 1 : advanced_hunting.contexts.size() - 1;
		}
	}

	advanced_hunting.selected_context = advanced_hunting.contexts[pos].context;
	advanced_hunting.selected = true;
	advanced_hunting.selected_match_frame = 0;
	advanced_hunting.selected_matches_this_frame = 0;
	LeaveCriticalSection(&G->mCriticalSection);
}

static void NextAdvancedContext(HackerDevice *, void *)
{
	SelectContext(true);
}

static void PrevAdvancedContext(HackerDevice *, void *)
{
	SelectContext(false);
}

static void ChangeAdvancedScope(bool next)
{
	EnterCriticalSectionPretty(&G->mCriticalSection);
	if (!advanced_hunting.active)
	{
		LeaveCriticalSection(&G->mCriticalSection);
		return;
	}

	int scope = static_cast<int>(advanced_hunting.scope);
	int count = static_cast<int>(AdvancedHuntingScope::DRAW) + 1;
	scope = next ? (scope + 1) % count : (scope + count - 1) % count;
	advanced_hunting.scope = static_cast<AdvancedHuntingScope>(scope);
	advanced_hunting.selected = false;
	advanced_hunting.context_limit_reached = false;
	advanced_hunting.contexts.clear();
	AdvancedHuntingScope new_scope = advanced_hunting.scope;
	LeaveCriticalSection(&G->mCriticalSection);

	LogOverlay(LOG_INFO, "> Context scope changed to %S; collecting a new context list\n",
	           AdvancedHuntingScopeName(new_scope));
}

static void NextAdvancedScope(HackerDevice *, void *)
{
	ChangeAdvancedScope(true);
}

static void PrevAdvancedScope(HackerDevice *, void *)
{
	ChangeAdvancedScope(false);
}

static void ToggleAdvancedCapture(HackerDevice *, void *)
{
	EnterCriticalSectionPretty(&G->mCriticalSection);
	if (!advanced_hunting.active)
	{
		LeaveCriticalSection(&G->mCriticalSection);
		return;
	}
	advanced_hunting.capture_locked = !advanced_hunting.capture_locked;
	bool locked = advanced_hunting.capture_locked;
	LeaveCriticalSection(&G->mCriticalSection);

	LogOverlay(LOG_INFO, "> Context capture %s\n", locked ? "locked" : "live");
}

static bool CopyContextTextToClipboard(const std::string &text)
{
	HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
	if (!memory)
		return false;

	void *data = GlobalLock(memory);
	if (!data)
	{
		GlobalFree(memory);
		return false;
	}
	memcpy(data, text.c_str(), text.size() + 1);
	GlobalUnlock(memory);

	if (!OpenClipboard(NULL))
	{
		GlobalFree(memory);
		return false;
	}
	EmptyClipboard();
	if (!SetClipboardData(CF_TEXT, memory))
	{
		CloseClipboard();
		GlobalFree(memory);
		return false;
	}
	CloseClipboard();
	return true;
}

static void MarkAdvancedContext(HackerDevice *device, void *)
{
	AdvancedHuntingOverlayInfo info;
	if (!GetAdvancedHuntingOverlayInfo(&info) || !info.selected)
	{
		LogOverlay(LOG_NOTICE, "> Select a draw context before marking it\n");
		return;
	}

	MarkSelectedShaderForAdvancedHunting(device, info.shader_stage == AdvancedHuntingShaderStage::PIXEL);

	std::string report = BuildAdvancedHuntingContextReport(info);
	LogInfo(">>>> Draw context marked: fingerprint = %016I64x\n%s", info.fingerprint, report.c_str());
	if (G->marking_actions & MarkingAction::CLIPBOARD)
	{
		if (CopyContextTextToClipboard(report))
			LogOverlay(LOG_INFO, "> Context matcher copied to clipboard\n");
		else
			LogOverlay(LOG_WARNING, "> Unable to copy context matcher to clipboard\n");
	}
}

static void ClearAdvancedHunting(HackerDevice *, void *)
{
	if (!AdvancedHuntingActive())
		return;
	ResetAdvancedHunting();
	LogOverlay(LOG_INFO, "> Returned to shader-level hunting\n");
}

static AdvancedHuntingScope ParseDefaultScope()
{
	wchar_t value[MAX_PATH];
	if (!GetIniStringAndLog(L"Hunting", L"context_scope", L"auto", value, MAX_PATH))
		return AdvancedHuntingScope::AUTO;
	if (!_wcsicmp(value, L"resource"))
		return AdvancedHuntingScope::RESOURCE;
	if (!_wcsicmp(value, L"draw"))
		return AdvancedHuntingScope::DRAW;
	if (_wcsicmp(value, L"auto"))
		LogOverlayW(LOG_WARNING, L"Unknown context_scope '%ls'; using auto\n", value);
	return AdvancedHuntingScope::AUTO;
}

void ParseAdvancedHuntingSection(int repeat)
{
	ResetAdvancedHunting();
	G->advanced_hunting_enabled = GetIniInt(L"Hunting", L"advanced_hunting", 0, NULL) != 0;
	if (!G->advanced_hunting_enabled)
		return;

	AdvancedHuntingScope default_scope = ParseDefaultScope();
	bool verbose = GetIniInt(L"Hunting", L"context_overlay_verbose", 0, NULL) != 0;
	size_t max_contexts =
	    static_cast<size_t>(max(1, min(4096, GetIniInt(L"Hunting", L"context_max_entries", 512, NULL))));
	unsigned lifetime_frames =
	    static_cast<unsigned>(max(0, GetIniInt(L"Hunting", L"context_lifetime_frames", 120, NULL)));

	EnterCriticalSectionPretty(&G->mCriticalSection);
	advanced_hunting.default_scope = default_scope;
	advanced_hunting.scope = advanced_hunting.default_scope;
	advanced_hunting.verbose = verbose;
	advanced_hunting.max_contexts = max_contexts;
	advanced_hunting.lifetime_frames = lifetime_frames;
	LeaveCriticalSection(&G->mCriticalSection);

	RegisterIniKeyBinding(L"Hunting", L"toggle_context_hunting", ToggleAdvancedHunting, NULL, 0, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_context", PrevAdvancedContext, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"next_context", NextAdvancedContext, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_context", MarkAdvancedContext, NULL, 0, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_context_scope", PrevAdvancedScope, NULL, 0, NULL);
	RegisterIniKeyBinding(L"Hunting", L"next_context_scope", NextAdvancedScope, NULL, 0, NULL);
	RegisterIniKeyBinding(L"Hunting", L"toggle_context_capture", ToggleAdvancedCapture, NULL, 0, NULL);
	RegisterIniKeyBinding(L"Hunting", L"clear_context_hunting", ClearAdvancedHunting, NULL, 0, NULL);
}
