"""Verify regression sensitivity against real native runtime behavior."""

import hashlib
import json
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / ".quality" / "runtime-mutations"
EXE = ROOT / ".quality" / "runtime-tests" / "x64" / "Release" / "RuntimeTests.exe"
BUILD = [
    "msbuild",
    "DirectX11/RuntimeTests.vcxproj",
    "-property:Configuration=Release",
    "-property:Platform=x64",
    f"-property:SolutionDir={ROOT}/",
    "-property:EdhmStrictBuild=true",
    "-target:Build",
    "-nodeReuse:false",
    "-verbosity:minimal",
    "-restore:false",
]


def run(command, log_name):
    result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=300)
    output = result.stdout + result.stderr
    (OUTPUT / log_name).write_bytes(output)
    return result.returncode, output.decode(errors="replace")


def verify_baselines(mutations):
    for case, _, _, _ in mutations:
        baseline_exit, _ = run([str(EXE), case], f"{case}-baseline.log")
        if baseline_exit:
            raise RuntimeError(f"Baseline must pass before mutation: {case}")


def get_mutations(command_list, settings, profiling):
    return [
        (
            "empty-groups",
            command_list,
            '\tif (tree->tokens.empty())\n\t\tthrow CommandListSyntaxError(L"Expression inside parentheses must not be empty", 0);',
            "",
        ),
        (
            "leading-dot",
            command_list,
            "std::isdigit(remain[0]) || remain[0] == L'.'",
            "std::isdigit(remain[0])",
        ),
        (
            "pending-save",
            settings,
            'bool SavePersistentSettings()\n{\n\tG->gSettingsSaveTime = G->gTime;',
            'bool SavePersistentSettings()\n{\n\tG->gSettingsSaveTime = G->gTime;\n\tG->user_config_dirty = 0;',
        ),
        (
            "pool-reset",
            command_list,
            "src.type == ResourceCopyTargetType::POOL || src.type == ResourceCopyTargetType::EMPTY",
            "src.type == ResourceCopyTargetType::POOL",
        ),
        (
            "constant-stride",
            command_list,
            "new_desc.StructureByteStride = 0;",
            "new_desc.StructureByteStride = 8;",
        ),
        (
            "reference-order",
            command_list,
            "DeferBindFlagsPropagation(src_custom_resource, dst.GetCustomResource(nullptr));",
            "DeferBindFlagsPropagation(nullptr, nullptr);",
        ),
        (
            "reference-cycles",
            command_list,
            "return pool->PropagateFlags(extra_bind_flags, extra_misc_flags);",
            "return pool->PropagateFlags(extra_bind_flags, extra_misc_flags);\n"
            "\tmisc_flags = (D3D11_RESOURCE_MISC_FLAG)(misc_flags | extra_misc_flags);",
        ),
        (
            "pool-metadata",
            command_list,
            "custom_resource =\n\t\t\t    state->pool_metadata_preview->GetPool(custom_resource_pool)->GetResource(id, false, false, true);",
            "custom_resource = custom_resource_pool->GetResource(id, false, false, false);",
        ),
        (
            "pool-identifiers",
            command_list,
            "const size_t pool_length = FindResourceCopyTargetTokenEnd(remain, 1);",
            "const size_t pool_length = FindVariableTokenEnd(remain, 1);",
        ),
        (
            "store-values",
            command_list,
            "copy_end = ((value_end + stride - 1) / stride) * stride;",
            "copy_offset = value_offset;\n\t\tcopy_end = value_end;",
        ),
        (
            "store-views",
            command_list,
            "src_resource.Attach(src.GetResource(state, src_view.GetAddressOf(), nullptr, nullptr, nullptr, nullptr, nullptr));",
            "src_resource.Attach(src.GetResource(state, src_view.GetAddressOf(), nullptr, nullptr, nullptr, nullptr, nullptr));\n"
            "\tif (src_view)\n\t\tsrc_view->AddRef();",
        ),
        (
            "store-failure",
            command_list,
            'LogInfo("StoreCommand: Failed to acquire readback buffer\\n");',
            'LogInfo("StoreCommand: Failed to acquire readback buffer\\n");\n\t\tvar->fval = 0;',
        ),
        (
            "region-copy",
            command_list,
            "UINT buf_dst_size = region_size;",
            "UINT buf_dst_size = 0;",
        ),
        (
            "region-views",
            command_list,
            "!region_size && !(options & ResourceCopyOptions::RAW_VIEW) && src_view &&",
            "!(options & ResourceCopyOptions::RAW_VIEW) && src_view &&",
        ),
        (
            "region-cb",
            command_list,
            "buf_src_size = region_extent;",
            "buf_src_size = region_extent;\n"
            "\t\tif (src.type == ResourceCopyTargetType::CONSTANT_BUFFER)\n"
            "\t\t\tbuf_dst_size = min(buf_dst_size, 256U);",
        ),
        (
            "settings-lifecycle",
            settings,
            "persistent_settings_state.Remember(*key, value);",
            'persistent_settings_state.Remember(L"$wrong", value);',
        ),
        (
            "settings-atomic",
            settings,
            "if (failed)\n\t{\n\t\tDeleteFileW(temporary);",
            "if (failed)\n\t{\n"
            "\t\tHANDLE damaged = CreateFileW(destination, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, "
            "nullptr, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);\n"
            "\t\tif (damaged != INVALID_HANDLE_VALUE)\n\t\t\tCloseHandle(damaged);\n"
            "\t\tDeleteFileW(temporary);",
        ),
        (
            "constants-order",
            settings,
            'ParseCommandList(L"Constants", &G->constants_command_list, &G->post_constants_command_list, nullptr);',
            'ParseCommandList(L"Constants", &G->constants_command_list, &G->post_constants_command_list, nullptr);\n'
            '\tstd::reverse(G->constants_command_list.commands.begin(), G->constants_command_list.commands.end());',
        ),
        (
            "ini-sections",
            settings,
            "if (warn_duplicates == 2)",
            "if (warn_duplicates != 2)",
        ),
        (
            "if-conditions",
            command_list,
            "if (expression.evaluate(state) != 0.0f)",
            "if (expression.evaluate(state) == 0.0f)",
        ),
        (
            "expression-grammar",
            command_list,
            "transform_operators_recursive(&tree, exponent_operators, ARRAYSIZE(exponent_operators), true, false);",
            "transform_operators_recursive(&tree, exponent_operators, ARRAYSIZE(exponent_operators), false, false);",
        ),
        (
            "profiling-collections",
            profiling,
            "Profiling::text.clear();\n\tProfiling::clear();\n}",
            "Profiling::text.clear();\n}",
        ),
        (
            "profiling-reload",
            settings,
            "// Drop profiling pointers before parsing can replace command storage.\n\tProfiling::clear();",
            "// Deliberate break: retain old profiling pointers during reload.",
        ),
        (
            "matching-diagnostics",
            ROOT / "DirectX11" / "ResourceHash.cpp",
            "if (matches->empty() || G->fuzzy_match_alongside_hash)",
            "if (matches->empty())",
        ),
        (
            "view-reuse",
            command_list,
            "return typed;",
            "return {};",
        ),
        (
            "view-descriptors",
            command_list,
            "if (!SameViewDesc(actual, expected))",
            "if (actual.Format == DXGI_FORMAT_UNKNOWN && !SameViewDesc(actual, expected))",
        ),
        (
            "view-cache-metadata",
            command_list,
            "*cached_view_parameters == requested_view",
            "cached_view_parameters->target == requested_view.target",
        ),
        (
            "view-ownership",
            command_list,
            "return typed;",
            "typed->AddRef();\n\treturn typed;",
        ),
        (
            "view-failure",
            command_list,
            "const auto type = EffectiveViewTarget(&dst, state);",
            "const auto type = dst_custom_resource ? EffectiveViewTarget(&dst, state) : ResourceCopyTargetType::CUSTOM_RESOURCE;",
        ),
        (
            'region-cb-small',
            command_list,
            'FillInMissingInfo(src.type, src_resource, src_view, &stride, &offset, &buf_src_size, &format, region_size);',
            'FillInMissingInfo(src.type, src_resource, src_view, &stride, &offset, &buf_src_size, &format, 0);',
        ),
        (
            'view-clear',
            command_list,
            'if (!view || !clear_unknown_view(view, state))',
            'if (!view)',
        ),
        (
            "pool-learned-flags",
            command_list,
            "const float id = pool_dynamic_index_expression->evaluate(state);\n\t\t\t"
            "custom_resource =\n\t\t\t    state->pool_metadata_preview->GetPool(custom_resource_pool)->GetResource(id, false, false, true);",
            "custom_resource = GetCustomResource(nullptr);",
        ),
        (
            'pool-scalar-precedence',
            command_list,
            "if (pool_length && remain[pool_length - 1] == L']')",
            'if (pool_length)',
        ),
        (
            'view-this-cache',
            command_list,
            'else if (cached_view_parameters && *cached_view_parameters == requested_view &&',
            'else if (dst.type != ResourceCopyTargetType::THIS_RESOURCE && cached_view_parameters && *cached_view_parameters == requested_view &&',
        ),
        (
            "profiling-zero-frame",
            profiling,
            "if (freeze || !frames)",
            "if (freeze)",
        ),
        (
            "missing-metadata",
            command_list,
            "*misc_flags = custom_resource ? custom_resource->misc_flags : static_cast<D3D11_RESOURCE_MISC_FLAG>(0);",
            "*misc_flags = custom_resource->misc_flags;",
        ),
        (
            "nested-metadata",
            command_list,
            "const MetadataScope metadata(state->pool_metadata_query);",
            "// Deliberate break: evaluate nested indexes with ordinary pool reads.",
        ),
        (
            "metadata-resource",
            command_list,
            "return state->pool_metadata_preview->GetPool(custom_resource_pool)\n\t\t\t\t\t\t\t      ->GetResource(id, template_lookup, false, assign);",
            "return custom_resource_pool->resource_template;",
        ),
        (
            "metadata-lazy-index",
            command_list,
            "element.resource->pool_index = static_cast<int>(pool_index);",
            "element.resource->pool_index = metadata_preview ? -1 : static_cast<int>(pool_index);",
        ),
        (
            "metadata-fifo",
            command_list,
            "result->allocate_slot_on_missing = allocate_slot_on_missing;",
            "result->allocate_slot_on_missing = false;",
        ),
        (
            "metadata-expiration",
            command_list,
            "result->last_expiration_run = last_expiration_run;",
            "result->last_expiration_run = UINT32_MAX;",
        ),
        (
            "metadata-resource-identity",
            command_list,
            "resource->AdoptMetadataResource(*preview);",
            "static_cast<void>(preview);",
        ),
        (
            "metadata-hot-identity",
            command_list,
            "\tif (preview.metadata_adopted || !preview.substantiated || resource != preview.metadata_original_resource ||\n\t    view != preview.metadata_original_view || device != preview.metadata_original_device ||\n\t    substantiated != preview.metadata_original_substantiated)\n\t\treturn;\n\tpreview.metadata_adopted = true;\n\t// Existing GPU objects and their live caches stay in place. Only\n\t// provisionally created or replaced runtime state needs transferring.\n\tif ((resource || view) && resource == preview.resource && view == preview.view)\n\t\treturn;\n",
            "\tif (!preview.substantiated || resource != preview.metadata_original_resource ||\n\t    view != preview.metadata_original_view || device != preview.metadata_original_device ||\n\t    substantiated != preview.metadata_original_substantiated)\n\t\treturn;\n",
        ),
        (
            "metadata-cached-spatial",
            command_list,
            "cached.cached_data_offset = handle_info->cached_data_offset;",
            "cached.cached_data_offset = 0;",
        ),
        (
            "metadata-static-pin",
            command_list,
            "if (owner && owner->ResolvePool() == owner && owner->MetadataResource(resource->pool_index) == resource)",
            "if (owner)",
        ),
        (
            "metadata-alias",
            command_list,
            "\t\tCustomResourcePool *owner = resource->pool;\n\t\tif (owner && owner->ResolvePool() == owner && owner->MetadataResource(resource->pool_index) == resource)\n\t\t\treturn GetPool(owner)->MetadataResource(resource->pool_index);\n",
            "",
        ),
        (
            "metadata-alias-expiration",
            command_list,
            "const MetadataCreationInfo created = preview.metadata_creation_info.value_or(",
            "preview.metadata_creation_info.reset();\n\tconst MetadataCreationInfo created = preview.metadata_creation_info.value_or(",
        ),
    ]


def restore_production(originals, mutations, evidence, original_failure):
    try:
        for path, content in originals.items():
            path.write_bytes(content)
        (OUTPUT / "results.json").write_text(json.dumps(evidence, indent=2) + "\n")
        build_exit, _ = run(BUILD, "restored-build.log")
        if build_exit:
            raise RuntimeError("Restored production source did not build")
        for case, _, _, _ in mutations:
            exit_code, _ = run([str(EXE), case], f"{case}-restored.log")
            if exit_code:
                raise RuntimeError(f"Restored production behavior failed: {case}")
    except BaseException as restoration_failure:
        # A restored checkout must never retain a binary built from a mutant.
        EXE.unlink(missing_ok=True)
        if original_failure is None:
            raise
        print(f"Production binary quarantined after cleanup failure: {restoration_failure}", file=sys.stderr)


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    command_list = ROOT / "DirectX11" / "CommandList.cpp"
    settings = ROOT / "DirectX11" / "IniHandler.cpp"
    mutations = get_mutations(command_list, settings, ROOT / "DirectX11" / "profiling.cpp")
    originals = {path: path.read_bytes() for _, path, _, _ in mutations}
    evidence = []
    try:
        verify_baselines(mutations)
        for case, path, old, new in mutations:
            original = originals[path].decode("utf-8").replace("\r\n", "\n")
            if original.count(old) != 1:
                raise RuntimeError(f"Mutation must match exactly one source location: {case}")
            mutated = original.replace(old, new, 1).encode("utf-8")
            path.write_bytes(mutated)
            try:
                build_exit, _ = run(BUILD, f"{case}-build.log")
                if build_exit:
                    raise RuntimeError(f"Mutation must compile successfully: {case}")
                exit_code, output = run([str(EXE), case], f"{case}-mutant.log")
                if not exit_code or f"RUN {case}" not in output:
                    raise RuntimeError(f"Regression check did not detect mutation: {case}")
                evidence.append(
                    {
                        "case": case,
                        "baseline_exit": 0,
                        "mutant_exit": exit_code,
                        "original_sha256": hashlib.sha256(originals[path]).hexdigest(),
                        "mutant_sha256": hashlib.sha256(mutated).hexdigest(),
                    }
                )
                print(f"Detected deliberate behavior break: {case} (exit {exit_code})", flush=True)
            finally:
                path.write_bytes(originals[path])
    finally:
        restore_production(originals, mutations, evidence, sys.exc_info()[1])
    print("All mutations detected; restored production behavior passes.", flush=True)


if __name__ == "__main__":
    main()
