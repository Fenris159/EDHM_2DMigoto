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
	Expect(report.find("match_first_index = 4") != std::string::npos, "draw scope reports zero-safe offsets");
}
} // namespace

int main()
{
	TestScopeIdentity();
	TestFingerprintUsesIdentity();
	TestReportIsConservative();
	if (failures)
		return 1;
	std::cout << "Advanced hunting tests passed\n";
	return 0;
}
