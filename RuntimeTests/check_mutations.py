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
            '\tconst std::string previous_locale = setlocale(LC_CTYPE, nullptr);',
            '\tG->user_config_dirty = 0;\n\tconst std::string previous_locale = setlocale(LC_CTYPE, nullptr);',
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
    ]
    originals = {path: path.read_bytes() for _, path, _, _ in mutations}
    evidence = []
    try:
        for case, path, old, new in mutations:
            baseline_exit, _ = run([str(EXE), case], f"{case}-baseline.log")
            if baseline_exit:
                raise RuntimeError(f"Baseline must pass before mutation: {case}")
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
