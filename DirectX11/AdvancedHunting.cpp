#include "AdvancedHunting.h"

#include <algorithm>
#include <cstring>

#include "globals.h"
#include "HackerContext.h"
#include "HackerDevice.h"
#include "Hunting.h"
#include "IniHandler.h"
#include "input.h"
#include "Overlay.h"
#include "log.h"

static AdvancedHuntingState advanced_hunting;

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
	EnterCriticalSectionPretty(&G->mCriticalSection);
	const bool configured = G->advanced_hunting_enabled;
	LeaveCriticalSection(&G->mCriticalSection);
	return configured;
}

bool AdvancedHuntingActive()
{
	EnterCriticalSectionPretty(&G->mCriticalSection);
	const bool active = advanced_hunting.Active();
	LeaveCriticalSection(&G->mCriticalSection);
	return active;
}

bool GetAdvancedHuntingStateForDraw(UINT64 vertex_shader, UINT64 pixel_shader, AdvancedHuntingShaderStage *stage,
                                    UINT64 *hash, bool *parent_matches)
{
	EnterCriticalSectionPretty(&G->mCriticalSection);
	const bool active = advanced_hunting.GetStateForDraw(vertex_shader, pixel_shader, stage, hash, parent_matches);
	LeaveCriticalSection(&G->mCriticalSection);
	return active;
}

bool SubmitAdvancedHuntingContext(const AdvancedHuntingContext &context)
{
	EnterCriticalSectionPretty(&G->mCriticalSection);
	const bool selected = advanced_hunting.Submit(context);
	LeaveCriticalSection(&G->mCriticalSection);
	return selected;
}

void NoteAdvancedHuntingDeferredContext()
{
	EnterCriticalSectionPretty(&G->mCriticalSection);
	advanced_hunting.NoteDeferredContext();
	LeaveCriticalSection(&G->mCriticalSection);
}

void AdvanceAdvancedHuntingFrame()
{
	EnterCriticalSectionPretty(&G->mCriticalSection);
	G->frame_no++;
	advanced_hunting.AdvanceFrame(G->frame_no);
	LeaveCriticalSection(&G->mCriticalSection);
}

void ResetAdvancedHunting()
{
	EnterCriticalSectionPretty(&G->mCriticalSection);
	advanced_hunting.Reset();
	LeaveCriticalSection(&G->mCriticalSection);
}

bool GetAdvancedHuntingOverlayInfo(AdvancedHuntingOverlayInfo *info)
{
	EnterCriticalSectionPretty(&G->mCriticalSection);
	const bool available = advanced_hunting.GetOverlayInfo(info);
	LeaveCriticalSection(&G->mCriticalSection);
	return available;
}

static void ToggleAdvancedHunting(HackerDevice *device, void *)
{
	if (G->hunting != HUNTING_MODE_ENABLED)
	{
		LogOverlay(LOG_NOTICE, "> Enable shader hunting before entering context hunting\n");
		return;
	}

	AdvancedHuntingShaderStage stage = AdvancedHuntingShaderStage::NONE;
	UINT64 hash = 0;
	unsigned frame_no = 0;
	EnterCriticalSectionPretty(&G->mCriticalSection);
	if (advanced_hunting.Active())
	{
		advanced_hunting.Reset();
		LeaveCriticalSection(&G->mCriticalSection);
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
	frame_no = G->frame_no;
	LeaveCriticalSection(&G->mCriticalSection);

	if (stage == AdvancedHuntingShaderStage::NONE)
	{
		LogOverlay(LOG_NOTICE, "> Select a pixel or vertex shader before entering context hunting\n");
		return;
	}

	HackerContext *context = device ? device->GetHackerContext() : nullptr;
	if (!context || !context->SnapshotAdvancedHuntingState())
	{
		LogOverlay(LOG_WARNING, "> Context hunting requires an immediate Direct3D context\n");
		return;
	}

	EnterCriticalSectionPretty(&G->mCriticalSection);
	advanced_hunting.Enter(stage, hash, frame_no);
	LeaveCriticalSection(&G->mCriticalSection);
	LogOverlay(LOG_INFO, "> Context hunting enabled for %S %016I64x\n", AdvancedHuntingStageName(stage), hash);
}

static void SelectContext(bool next)
{
	EnterCriticalSectionPretty(&G->mCriticalSection);
	const bool selected = advanced_hunting.Select(next);
	LeaveCriticalSection(&G->mCriticalSection);
	if (!selected)
		LogOverlay(LOG_NOTICE, "> No draw contexts have been observed for the selected shader\n");
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
	AdvancedHuntingScope scope = AdvancedHuntingScope::AUTO;
	EnterCriticalSectionPretty(&G->mCriticalSection);
	const bool changed = advanced_hunting.ChangeScope(next, &scope);
	LeaveCriticalSection(&G->mCriticalSection);
	if (changed)
		LogOverlay(LOG_INFO, "> Context scope changed to %S; collecting a new context list\n",
		           AdvancedHuntingScopeName(scope));
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
	bool locked = false;
	EnterCriticalSectionPretty(&G->mCriticalSection);
	const bool changed = advanced_hunting.ToggleCapture(&locked);
	LeaveCriticalSection(&G->mCriticalSection);
	if (changed)
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
	if (!OpenClipboard(nullptr))
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
	const std::string report = BuildAdvancedHuntingContextReport(info);
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
	if (_wcsicmp(value, L"resource") == 0)
		return AdvancedHuntingScope::RESOURCE;
	if (_wcsicmp(value, L"draw") == 0)
		return AdvancedHuntingScope::DRAW;
	if (_wcsicmp(value, L"auto") != 0)
		LogOverlayW(LOG_WARNING, L"Unknown context_scope '%ls'; using auto\n", value);
	return AdvancedHuntingScope::AUTO;
}

void ParseAdvancedHuntingSection(int repeat)
{
	const bool enabled = GetIniInt(L"Hunting", L"advanced_hunting", 0, nullptr) != 0;
	const AdvancedHuntingScope default_scope = enabled ? ParseDefaultScope() : AdvancedHuntingScope::AUTO;
	const bool verbose = enabled && GetIniInt(L"Hunting", L"context_overlay_verbose", 0, nullptr) != 0;
	const size_t max_contexts =
	    enabled ? static_cast<size_t>(
	                  (std::max)(1, (std::min)(4096, GetIniInt(L"Hunting", L"context_max_entries", 512, nullptr))))
		        : 512;
	const unsigned lifetime_frames =
	    enabled ? static_cast<unsigned>((std::max)(0, GetIniInt(L"Hunting", L"context_lifetime_frames", 120, nullptr)))
		        : 120;

	EnterCriticalSectionPretty(&G->mCriticalSection);
	G->advanced_hunting_enabled = enabled;
	advanced_hunting.Reset();
	advanced_hunting.Configure(default_scope, verbose, max_contexts, lifetime_frames);
	LeaveCriticalSection(&G->mCriticalSection);
	if (!enabled)
		return;

	RegisterIniKeyBinding(L"Hunting", L"toggle_context_hunting", ToggleAdvancedHunting, nullptr, 0, nullptr);
	RegisterIniKeyBinding(L"Hunting", L"previous_context", PrevAdvancedContext, nullptr, repeat, nullptr);
	RegisterIniKeyBinding(L"Hunting", L"next_context", NextAdvancedContext, nullptr, repeat, nullptr);
	RegisterIniKeyBinding(L"Hunting", L"mark_context", MarkAdvancedContext, nullptr, 0, nullptr);
	RegisterIniKeyBinding(L"Hunting", L"previous_context_scope", PrevAdvancedScope, nullptr, 0, nullptr);
	RegisterIniKeyBinding(L"Hunting", L"next_context_scope", NextAdvancedScope, nullptr, 0, nullptr);
	RegisterIniKeyBinding(L"Hunting", L"toggle_context_capture", ToggleAdvancedCapture, nullptr, 0, nullptr);
	RegisterIniKeyBinding(L"Hunting", L"clear_context_hunting", ClearAdvancedHunting, nullptr, 0, nullptr);
}
