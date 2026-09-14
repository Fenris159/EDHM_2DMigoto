#include "../DirectX11/AdvancedHunting.h"

#include <iostream>
#include <string>

namespace
{
int failures = 0;

void Expect(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}

AdvancedHuntingContext SampleContext()
{
	AdvancedHuntingContext context;
	context.shader_stage = AdvancedHuntingShaderStage::PIXEL;
	context.shader_hash = 0x0123456789abcdefull;
	context.draw_type = DrawCall::DrawIndexedInstanced;
	context.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	context.vertex_count = 12;
	context.index_count = 24;
	context.instance_count = 2;
	context.first_vertex = 3;
	context.first_index = 4;
	context.first_instance = 5;
	context.index_buffer = 0x11111111;
	context.vertex_buffers[2] = 0x22222222;
	context.pixel_shader_resources[7] = 0x33333333;
	context.render_targets[0] = 0x44444444;
	context.depth_target = 0x55555555;
	context.indirect_buffer = 0x66666666;
	context.indirect_args_offset = 32;
	return context;
}

void TestScopeIdentity()
{
	const AdvancedHuntingContext original = SampleContext();
	AdvancedHuntingContext changed = original;
	Expect(CompareAdvancedHuntingContexts(original, changed, AdvancedHuntingScope::DRAW) == 0,
	       "identical contexts compare equal");

	changed.first_index++;
	Expect(CompareAdvancedHuntingContexts(original, changed, AdvancedHuntingScope::RESOURCE) == 0,
	       "resource scope ignores draw offsets");
	Expect(CompareAdvancedHuntingContexts(original, changed, AdvancedHuntingScope::AUTO) == 0,
	       "auto scope ignores draw offsets");
	Expect(CompareAdvancedHuntingContexts(original, changed, AdvancedHuntingScope::DRAW) != 0,
	       "draw scope includes draw offsets");

	changed = original;
	changed.index_count++;
	Expect(CompareAdvancedHuntingContexts(original, changed, AdvancedHuntingScope::RESOURCE) == 0,
	       "resource scope ignores draw counts");
	Expect(CompareAdvancedHuntingContexts(original, changed, AdvancedHuntingScope::AUTO) != 0,
	       "auto scope includes draw counts");

	changed = original;
	changed.pixel_shader_resources[7]++;
	Expect(CompareAdvancedHuntingContexts(original, changed, AdvancedHuntingScope::RESOURCE) != 0,
	       "resource scope includes bound resources");
}

void TestFingerprintUsesIdentity()
{
	const AdvancedHuntingContext original = SampleContext();
	AdvancedHuntingContext changed = original;
	changed.first_vertex++;
	Expect(FingerprintAdvancedHuntingContext(original, AdvancedHuntingScope::AUTO) ==
	           FingerprintAdvancedHuntingContext(changed, AdvancedHuntingScope::AUTO),
	       "auto fingerprints ignore draw offsets");
	Expect(FingerprintAdvancedHuntingContext(original, AdvancedHuntingScope::DRAW) !=
	           FingerprintAdvancedHuntingContext(changed, AdvancedHuntingScope::DRAW),
	       "draw fingerprints include draw offsets");
}

void TestReportIsConservative()
{
	AdvancedHuntingOverlayInfo info;
	info.scope = AdvancedHuntingScope::DRAW;
	info.context = SampleContext();
	info.fingerprint = FingerprintAdvancedHuntingContext(info.context, info.scope);
	const std::string report = BuildAdvancedHuntingContextReport(info);

	Expect(report.find("Parent PS 0123456789abcdef") != std::string::npos, "report includes the parent shader");
	Expect(report.find("Resource ps-t7 = 33333333") != std::string::npos, "report includes the observed slot");
	Expect(report.find("Candidate only") != std::string::npos, "report labels the INI candidate as inexact");
	Expect(report.find("[ShaderOverride-ContextParent-") != std::string::npos,
	       "report pairs the resource candidate with its parent shader");
	Expect(report.find("match_first_index = 4") != std::string::npos, "draw scope reports offsets");
	info.context.first_vertex = 0;
	info.context.first_index = 0;
	info.context.first_instance = 0;
	const std::string zero_report = BuildAdvancedHuntingContextReport(info);
	Expect(zero_report.find("match_first_index = 0") != std::string::npos, "draw scope reports a zero first index");
	Expect(zero_report.find("match_first_vertex = 0") != std::string::npos, "draw scope reports a zero first vertex");
	Expect(zero_report.find("match_first_instance = 0") != std::string::npos,
	       "draw scope reports a zero first instance");
}

void TestStateLifecycle()
{
	AdvancedHuntingState state;
	state.Configure(AdvancedHuntingScope::AUTO, true, 2, 2);
	state.Enter(AdvancedHuntingShaderStage::PIXEL, SampleContext().shader_hash, 10);
	bool parent_matches = false;
	Expect(state.GetStateForDraw(0, SampleContext().shader_hash, nullptr, nullptr, &parent_matches) && parent_matches,
	       "active state recognizes its parent shader");
	Expect(!state.Submit(SampleContext()), "an unselected context is not a selection match");
	Expect(state.Select(true), "the first observed context can be selected");
	Expect(state.Submit(SampleContext()), "the selected context matches subsequent draws");
	AdvancedHuntingOverlayInfo info;
	Expect(state.GetOverlayInfo(&info), "active state provides overlay data");
	Expect(info.context_count == 1 && info.observations == 2 && info.observed_frames == 1,
	       "duplicate observations update the existing entry");
	Expect(info.matches_this_frame == 1, "selected matches are counted in the current frame");
	state.AdvanceFrame(11);
	Expect(state.Submit(SampleContext()), "selected context remains matched on a later frame");
	state.GetOverlayInfo(&info);
	Expect(info.observed_frames == 2 && info.matches_this_frame == 1, "frame transition updates frame statistics");
	bool locked = false;
	Expect(state.ToggleCapture(&locked) && locked, "capture can be locked");
	AdvancedHuntingContext second = SampleContext();
	second.pixel_shader_resources[7]++;
	state.Submit(second);
	state.GetOverlayInfo(&info);
	Expect(info.context_count == 1, "capture lock rejects new contexts");
	Expect(state.ToggleCapture(&locked) && !locked, "capture can be resumed");
	state.Submit(second);
	state.GetOverlayInfo(&info);
	Expect(info.context_count == 2, "resumed capture accepts new contexts");
	AdvancedHuntingScope scope = AdvancedHuntingScope::AUTO;
	Expect(state.ChangeScope(true, &scope) && scope == AdvancedHuntingScope::RESOURCE,
	       "scope navigation advances from auto to resource");
	state.GetOverlayInfo(&info);
	Expect(info.context_count == 0 && !info.selected, "scope changes clear incompatible captured state");
	state.Submit(SampleContext());
	state.AdvanceFrame(14);
	state.GetOverlayInfo(&info);
	Expect(info.context_count == 0, "frame advancement prunes stale contexts without a parent draw");
	state.NoteDeferredContext();
	state.GetOverlayInfo(&info);
	Expect(info.deferred_context_seen, "deferred-context activity is exposed to the overlay");
	state.Reset();
	Expect(!state.GetOverlayInfo(&info), "reset disables and clears the state");
	state.Configure(AdvancedHuntingScope::RESOURCE, false, 1, 0);
	state.Enter(AdvancedHuntingShaderStage::PIXEL, SampleContext().shader_hash, 20);
	state.GetOverlayInfo(&info);
	Expect(info.scope == AdvancedHuntingScope::RESOURCE && info.context_count == 0,
	       "reconfiguration starts with a clean registry and updated defaults");
}

void TestContextTypePolicy()
{
	Expect(AdvancedHuntingSupportsContextType(D3D11_DEVICE_CONTEXT_IMMEDIATE),
	       "immediate contexts support advanced hunting");
	Expect(!AdvancedHuntingSupportsContextType(D3D11_DEVICE_CONTEXT_DEFERRED),
	       "deferred contexts are rejected instead of captured at record time");
}
} // namespace

int main()
{
	TestScopeIdentity();
	TestFingerprintUsesIdentity();
	TestReportIsConservative();
	TestStateLifecycle();
	TestContextTypePolicy();
	if (failures)
		return 1;
	std::cout << "Advanced hunting tests passed\n";
	return 0;
}
