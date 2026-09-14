#include "AdvancedHunting.h"

#include <algorithm>
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
      deferred_context_seen(false), shader_stage(AdvancedHuntingShaderStage::NONE), scope(AdvancedHuntingScope::AUTO),
      shader_hash(0), context_position(0), context_count(0), matches_this_frame(0), observed_frames(0), observations(0),
      fingerprint(0)
{
}

bool AdvancedHuntingState::ContextLess(const ContextEntry &lhs, const ContextEntry &rhs) const
{
	return CompareAdvancedHuntingContexts(lhs.context, rhs.context, scope_) < 0;
}

bool AdvancedHuntingState::ContextEquals(const AdvancedHuntingContext &lhs, const AdvancedHuntingContext &rhs) const
{
	return CompareAdvancedHuntingContexts(lhs, rhs, scope_) == 0;
}

std::vector<AdvancedHuntingState::ContextEntry>::iterator AdvancedHuntingState::FindContext(
    const AdvancedHuntingContext &context)
{
	ContextEntry key;
	key.context = context;
	auto less = [this](const ContextEntry &lhs, const ContextEntry &rhs) { return ContextLess(lhs, rhs); };
	auto it = std::lower_bound(contexts_.begin(), contexts_.end(), key, less);
	return it != contexts_.end() && ContextEquals(it->context, context) ? it : contexts_.end();
}

std::vector<AdvancedHuntingState::ContextEntry>::const_iterator AdvancedHuntingState::FindContext(
    const AdvancedHuntingContext &context) const
{
	ContextEntry key;
	key.context = context;
	auto less = [this](const ContextEntry &lhs, const ContextEntry &rhs) { return ContextLess(lhs, rhs); };
	auto it = std::lower_bound(contexts_.begin(), contexts_.end(), key, less);
	return it != contexts_.end() && ContextEquals(it->context, context) ? it : contexts_.end();
}

void AdvancedHuntingState::Configure(AdvancedHuntingScope default_scope, bool verbose, size_t max_contexts,
                                     unsigned lifetime_frames)
{
	default_scope_ = default_scope;
	scope_ = default_scope;
	verbose_ = verbose;
	max_contexts_ = std::max<size_t>(1, max_contexts);
	lifetime_frames_ = lifetime_frames;
}

void AdvancedHuntingState::Reset()
{
	active_ = false;
	capture_locked_ = false;
	selected_ = false;
	context_limit_reached_ = false;
	deferred_context_seen_ = false;
	shader_stage_ = AdvancedHuntingShaderStage::NONE;
	shader_hash_ = 0;
	current_frame_ = 0;
	selected_match_frame_ = 0;
	selected_matches_this_frame_ = 0;
	selected_context_ = AdvancedHuntingContext();
	contexts_.clear();
}

void AdvancedHuntingState::Enter(AdvancedHuntingShaderStage stage, UINT64 hash, unsigned frame_no)
{
	Reset();
	active_ = true;
	shader_stage_ = stage;
	shader_hash_ = hash;
	scope_ = default_scope_;
	current_frame_ = frame_no;
}

bool AdvancedHuntingState::Active() const
{
	return active_;
}

bool AdvancedHuntingState::GetStateForDraw(UINT64 vertex_shader, UINT64 pixel_shader, AdvancedHuntingShaderStage *stage,
                                           UINT64 *hash, bool *parent_matches) const
{
	bool matches = false;
	if (active_)
	{
		matches = shader_stage_ == AdvancedHuntingShaderStage::PIXEL ? pixel_shader == shader_hash_
		                                                             : vertex_shader == shader_hash_;
		if (stage)
			*stage = shader_stage_;
		if (hash)
			*hash = shader_hash_;
	}
	if (parent_matches)
		*parent_matches = matches;
	return active_;
}

void AdvancedHuntingState::PruneStaleContexts()
{
	if (capture_locked_ || !lifetime_frames_)
		return;
	contexts_.erase(std::remove_if(contexts_.begin(), contexts_.end(), [this](const ContextEntry &entry)
	                               { return current_frame_ - entry.last_seen_frame > lifetime_frames_; }),
	                contexts_.end());
	if (selected_ && FindContext(selected_context_) == contexts_.end())
		selected_ = false;
	if (contexts_.size() < max_contexts_)
		context_limit_reached_ = false;
}

void AdvancedHuntingState::AdvanceFrame(unsigned frame_no)
{
	current_frame_ = frame_no;
	PruneStaleContexts();
}

bool AdvancedHuntingState::Submit(const AdvancedHuntingContext &context)
{
	if (!active_ || context.shader_stage != shader_stage_ || context.shader_hash != shader_hash_)
		return false;
	auto it = FindContext(context);
	if (it != contexts_.end())
	{
		it->observations++;
		if (it->last_seen_frame != current_frame_)
		{
			it->last_seen_frame = current_frame_;
			it->observed_frames++;
		}
	}
	else if (!capture_locked_)
	{
		if (contexts_.size() >= max_contexts_)
			context_limit_reached_ = true;
		else
		{
			ContextEntry entry;
			entry.context = context;
			entry.last_seen_frame = current_frame_;
			entry.observed_frames = 1;
			entry.observations = 1;
			auto less = [this](const ContextEntry &lhs, const ContextEntry &rhs) { return ContextLess(lhs, rhs); };
			auto insert_at = std::lower_bound(contexts_.begin(), contexts_.end(), entry, less);
			contexts_.insert(insert_at, entry);
		}
	}
	const bool selected = selected_ && ContextEquals(context, selected_context_);
	if (selected)
	{
		if (selected_match_frame_ != current_frame_)
		{
			selected_match_frame_ = current_frame_;
			selected_matches_this_frame_ = 0;
		}
		selected_matches_this_frame_++;
	}
	return selected;
}

bool AdvancedHuntingState::Select(bool next)
{
	if (!active_ || contexts_.empty())
		return false;
	size_t pos = next ? 0 : contexts_.size() - 1;
	if (selected_)
	{
		auto it = FindContext(selected_context_);
		if (it != contexts_.end())
		{
			pos = static_cast<size_t>(std::distance(contexts_.begin(), it));
			pos = next ? (pos + 1) % contexts_.size() : (pos ? pos - 1 : contexts_.size() - 1);
		}
	}
	selected_context_ = contexts_[pos].context;
	selected_ = true;
	selected_match_frame_ = 0;
	selected_matches_this_frame_ = 0;
	return true;
}

bool AdvancedHuntingState::ChangeScope(bool next, AdvancedHuntingScope *scope)
{
	if (!active_)
		return false;
	int value = static_cast<int>(scope_);
	const int count = static_cast<int>(AdvancedHuntingScope::DRAW) + 1;
	value = next ? (value + 1) % count : (value + count - 1) % count;
	scope_ = static_cast<AdvancedHuntingScope>(value);
	selected_ = false;
	context_limit_reached_ = false;
	contexts_.clear();
	if (scope)
		*scope = scope_;
	return true;
}

bool AdvancedHuntingState::ToggleCapture(bool *locked)
{
	if (!active_)
		return false;
	capture_locked_ = !capture_locked_;
	if (locked)
		*locked = capture_locked_;
	return true;
}

void AdvancedHuntingState::NoteDeferredContext()
{
	if (active_)
		deferred_context_seen_ = true;
}

bool AdvancedHuntingState::GetOverlayInfo(AdvancedHuntingOverlayInfo *info) const
{
	if (!info || !active_)
		return false;
	*info = AdvancedHuntingOverlayInfo();
	info->active = true;
	info->capture_locked = capture_locked_;
	info->verbose = verbose_;
	info->selected = selected_;
	info->context_limit_reached = context_limit_reached_;
	info->deferred_context_seen = deferred_context_seen_;
	info->shader_stage = shader_stage_;
	info->scope = scope_;
	info->shader_hash = shader_hash_;
	info->context_count = contexts_.size();
	info->matches_this_frame = selected_match_frame_ == current_frame_ ? selected_matches_this_frame_ : 0;
	if (selected_)
	{
		auto it = FindContext(selected_context_);
		if (it != contexts_.end())
		{
			info->context_position = static_cast<size_t>(std::distance(contexts_.begin(), it)) + 1;
			info->observed_frames = it->observed_frames;
			info->observations = it->observations;
		}
		info->context = selected_context_;
		info->fingerprint = FingerprintAdvancedHuntingContext(selected_context_, scope_);
	}
	return true;
}

bool AdvancedHuntingSupportsContextType(D3D11_DEVICE_CONTEXT_TYPE type)
{
	return type == D3D11_DEVICE_CONTEXT_IMMEDIATE;
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
