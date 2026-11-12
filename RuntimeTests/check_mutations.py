"""Verify regression sensitivity against real native runtime behavior."""

import hashlib
import json
from pathlib import Path
import subprocess


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


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    command_list = ROOT / "DirectX11" / "CommandList.cpp"
    settings = ROOT / "DirectX11" / "IniHandler.cpp"
    mutations = [
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
            "CustomResource *custom_resource = GetCustomResource(nullptr);\n\t\tif (misc_flags)",
            "CustomResource *custom_resource = GetCustomResource(state);\n\t\tif (misc_flags)",
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
            "!region_size && src_view && (EquivTarget(src.type) == EquivTarget(dst.type))",
            "src_view && (EquivTarget(src.type) == EquivTarget(dst.type))",
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
    ]
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
    print("All mutations detected; restored production behavior passes.", flush=True)


if __name__ == "__main__":
    main()
