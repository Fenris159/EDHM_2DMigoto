#include "AdvancedHunting.h"

#include <cstring>
#include <iomanip>
#include <sstream>

namespace
{
template <typename Visitor, typename T> bool VisitField(const T &lhs, const T &rhs, Visitor &visitor)
{
	return visitor(&lhs, &rhs, sizeof(T));
}

template <typename Visitor>
void VisitContextIdentity(const AdvancedHuntingContext &lhs, const AdvancedHuntingContext &rhs,
                          AdvancedHuntingScope scope, Visitor visitor)
{
#define VISIT_CONTEXT_FIELD(field)                                                                                     \
	if (!VisitField(lhs.field, rhs.field, visitor))                                                                    \
	return

	VISIT_CONTEXT_FIELD(shader_stage);
	VISIT_CONTEXT_FIELD(shader_hash);
	VISIT_CONTEXT_FIELD(index_buffer);
	VISIT_CONTEXT_FIELD(indirect_buffer);
	VISIT_CONTEXT_FIELD(vertex_buffers);
	VISIT_CONTEXT_FIELD(pixel_shader_resources);
	VISIT_CONTEXT_FIELD(render_targets);
	VISIT_CONTEXT_FIELD(depth_target);
	if (scope == AdvancedHuntingScope::RESOURCE)
		return;

	VISIT_CONTEXT_FIELD(draw_type);
	VISIT_CONTEXT_FIELD(topology);
	VISIT_CONTEXT_FIELD(vertex_count);
	VISIT_CONTEXT_FIELD(index_count);
	VISIT_CONTEXT_FIELD(instance_count);
	if (scope == AdvancedHuntingScope::AUTO)
		return;

	VISIT_CONTEXT_FIELD(first_vertex);
	VISIT_CONTEXT_FIELD(first_index);
	VISIT_CONTEXT_FIELD(first_instance);
	VISIT_CONTEXT_FIELD(indirect_args_offset);

#undef VISIT_CONTEXT_FIELD
}

uint64_t HashBytes(uint64_t hash, const void *data, size_t size)
{
	const unsigned char *bytes = static_cast<const unsigned char *>(data);
	for (size_t i = 0; i < size; ++i)
	{
		hash ^= bytes[i];
		hash *= 1099511628211ull;
	}
	return hash;
}

void AppendResource(std::ostringstream &report, const char *slot, uint32_t hash)
{
	if (hash)
		report << "; Resource " << slot << " = " << std::hex << std::setw(8) << hash << "\r\n";
}
} // namespace

AdvancedHuntingContext::AdvancedHuntingContext()
    : shader_stage(AdvancedHuntingShaderStage::NONE), shader_hash(0), draw_type(DrawCall::Invalid),
      topology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED), vertex_count(0), index_count(0), instance_count(0), first_vertex(0),
      first_index(0), first_instance(0), index_buffer(0), vertex_buffers{}, pixel_shader_resources{}, render_targets{},
      depth_target(0), indirect_buffer(0), indirect_args_offset(0)
{
}

AdvancedHuntingOverlayInfo::AdvancedHuntingOverlayInfo()
    : active(false), capture_locked(false), verbose(false), selected(false), context_limit_reached(false),
      shader_stage(AdvancedHuntingShaderStage::NONE), scope(AdvancedHuntingScope::AUTO), shader_hash(0),
      context_position(0), context_count(0), matches_this_frame(0), observed_frames(0), observations(0), fingerprint(0)
{
}

int CompareAdvancedHuntingContexts(const AdvancedHuntingContext &lhs, const AdvancedHuntingContext &rhs,
                                   AdvancedHuntingScope scope)
{
	int result = 0;
	VisitContextIdentity(lhs, rhs, scope,
	                     [&result](const void *lhs_value, const void *rhs_value, size_t size)
	                     {
		                     result = std::memcmp(lhs_value, rhs_value, size);
		                     return result == 0;
	                     });
	return result;
}

uint64_t FingerprintAdvancedHuntingContext(const AdvancedHuntingContext &context, AdvancedHuntingScope scope)
{
	uint64_t hash = 14695981039346656037ull;
	VisitContextIdentity(context, context, scope,
	                     [&hash](const void *value, const void *, size_t size)
	                     {
		                     hash = HashBytes(hash, value, size);
		                     return true;
	                     });
	return hash;
}

std::string BuildAdvancedHuntingContextReport(const AdvancedHuntingOverlayInfo &info)
{
	const AdvancedHuntingContext &context = info.context;
	std::ostringstream report;
	uint32_t resource_hash = context.index_buffer;
	std::string resource_slot = "ib";

	if (!resource_hash && context.indirect_buffer)
	{
		resource_hash = context.indirect_buffer;
		resource_slot = "indirect-args";
	}
	if (!resource_hash)
	{
		for (UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
		{
			if (context.pixel_shader_resources[i])
			{
				resource_hash = context.pixel_shader_resources[i];
				resource_slot = "ps-t" + std::to_string(i);
				break;
			}
		}
	}
	if (!resource_hash)
	{
		for (UINT i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i)
		{
			if (context.vertex_buffers[i])
			{
				resource_hash = context.vertex_buffers[i];
				resource_slot = "vb" + std::to_string(i);
				break;
			}
		}
	}

	report << std::setfill('0');
	report << "; Advanced shader hunting context\r\n";
	report << "; Parent " << (context.shader_stage == AdvancedHuntingShaderStage::PIXEL ? "PS " : "VS ") << std::hex
	       << std::setw(16) << context.shader_hash << "\r\n";
	report << "; Scope ";
	switch (info.scope)
	{
	case AdvancedHuntingScope::RESOURCE:
		report << "RESOURCE\r\n";
		break;
	case AdvancedHuntingScope::DRAW:
		report << "DRAW\r\n";
		break;
	default:
		report << "AUTO\r\n";
		break;
	}
	report << "; Draw type=" << std::dec << static_cast<int>(context.draw_type)
	       << ", topology=" << static_cast<int>(context.topology) << ", vertex_count=" << context.vertex_count
	       << ", index_count=" << context.index_count << ", instance_count=" << context.instance_count << "\r\n";
	report << "; first_vertex=" << context.first_vertex << ", first_index=" << context.first_index
	       << ", first_instance=" << context.first_instance << ", indirect_args_offset=" << context.indirect_args_offset
	       << "\r\n";
	report << "; Fingerprint " << std::hex << std::setw(16) << info.fingerprint << "\r\n";

	AppendResource(report, "ib", context.index_buffer);
	AppendResource(report, "indirect-args", context.indirect_buffer);
	for (UINT i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i)
	{
		if (context.vertex_buffers[i])
			AppendResource(report, ("vb" + std::to_string(i)).c_str(), context.vertex_buffers[i]);
	}
	for (UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
	{
		if (context.pixel_shader_resources[i])
			AppendResource(report, ("ps-t" + std::to_string(i)).c_str(), context.pixel_shader_resources[i]);
	}
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
	{
		if (context.render_targets[i])
			AppendResource(report, ("o" + std::to_string(i)).c_str(), context.render_targets[i]);
	}
	AppendResource(report, "depth", context.depth_target);

	if (resource_hash)
	{
		report << "\r\n; Candidate only: TextureOverride cannot encode the complete context identity.\r\n";
		report << "; Keep the parent shader and observed " << resource_slot
		       << " binding constraint when integrating.\r\n";
		report << "[TextureOverride-ContextResource-" << std::hex << std::setw(16) << info.fingerprint << "]\r\n";
		report << "hash = " << std::setw(8) << resource_hash << "\r\n";
		if (context.index_count)
			report << "match_index_count = " << std::dec << context.index_count << "\r\n";
		if (context.vertex_count)
			report << "match_vertex_count = " << context.vertex_count << "\r\n";
		if (context.instance_count)
			report << "match_instance_count = " << context.instance_count << "\r\n";
		if (info.scope == AdvancedHuntingScope::DRAW)
		{
			report << "match_first_index = " << context.first_index << "\r\n";
			report << "match_first_vertex = " << context.first_vertex << "\r\n";
			report << "match_first_instance = " << context.first_instance << "\r\n";
		}
		report << "filter_index = <assign>\r\n\r\n";
		report << "[ShaderOverride-ContextParent-" << std::hex << std::setw(16) << info.fingerprint << "]\r\n";
		report << "hash = " << std::setw(16) << context.shader_hash << "\r\n";
		report << "; Gate the intended action on " << resource_slot << " == <assign>.\r\n";
	}
	else
	{
		report << "\r\n; No stable resource hash was observed. Use the full context report for diagnosis.\r\n";
	}

	return report.str();
}
