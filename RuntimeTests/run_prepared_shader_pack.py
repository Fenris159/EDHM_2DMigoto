"""Collect native shader-object acceptance from pinned public inputs, without publishing assets."""

import argparse
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile
import urllib.request
import zipfile


ROOT = Path(__file__).resolve().parents[1]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    sys.modules[name] = result
    spec.loader.exec_module(result)
    return result


preparer = module("pack_preparer", ROOT / "scripts/prepare-edhm-dxbc.py")
inventory = module("pack_inventory", ROOT / "scripts/inventory-edhm.py")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def verified(data, record):
    if len(data) != record["bytes"] or sha(data) != record["sha256"]:
        raise ValueError("pinned size/digest mismatch")
    return data


def download(record):
    size = record["bytes"]
    if not isinstance(size, int) or not 0 < size <= 16 * 1024 * 1024:
        raise ValueError("archive exceeds collection limit")
    if not record["url"].startswith(("https://raw.githubusercontent.com/psychicEgg/EDHM/",
                                     "https://github.com/bo3b/3Dmigoto/releases/download/")):
        raise ValueError("unexpected fixture origin")
    with urllib.request.urlopen(record["url"], timeout=60) as response:
        return verified(response.read(size + 1), record)


def collect(args):
    recipe_path = ROOT / "RuntimeTests/fixtures/public-shader-inputs.json"
    tool_path = ROOT / "RuntimeTests/fixtures/public-assembler-inputs.json"
    recipe_bytes, tool_bytes = recipe_path.read_bytes(), tool_path.read_bytes()
    recipe, tool_recipe = json.loads(recipe_bytes), json.loads(tool_bytes)
    if recipe["schema_version"] != 1 or tool_recipe["schema_version"] != 1 or len(recipe["files"]) != 77:
        raise ValueError("unexpected fixture recipe")
    checker = args.checker.resolve()
    checker_bytes = checker.read_bytes()
    if (args.wine is None) != (args.wine_prefix is None) or (os.name != "nt" and not args.wine):
        raise ValueError("non-Windows collection needs Wine and an explicit isolated prefix")
    receipt = {
        "kind": "off-game-shader-object-observations", "status": "collecting",
        "source": os.environ.get("GITHUB_SHA"), "host": platform.platform(),
        "route": "wine-requested-warp" if args.wine else "windows-system-d3d11-warp",
        "checker_machine": hex(preparer.pe_machine(checker_bytes)),
        "checker_sha256": sha(checker_bytes), "runner_sha256": sha(Path(__file__).read_bytes()),
        "shader_recipe_sha256": sha(recipe_bytes), "tool_recipe_sha256": sha(tool_bytes),
        "original_identity": "unverified", "rendered_output": "not_tested", "assets_redistributed": False,
        "observations": [],
    }
    with tempfile.TemporaryDirectory(prefix="edhm-shader-pack-") as temporary:
        work = Path(temporary)
        root, tools = work / "inputs", work / "tools"
        (root / "ShaderFixes").mkdir(parents=True)
        (root / "ShaderCache").mkdir()
        tools.mkdir()
        archives = {r["id"]: download(r) for r in recipe["archives"]}
        for record in recipe["files"]:
            if not re.fullmatch(r"ShaderFixes/[0-9a-f]{16}-(ps|vs)\.txt", record["path"]):
                raise ValueError("unexpected fixture destination")
            if not 0 < record["bytes"] <= 1024 * 1024 or record["stage"] not in ("ps", "vs"):
                raise ValueError("unexpected shader extent/stage")
            with zipfile.ZipFile(io.BytesIO(archives[record["archive"]])) as archive:
                members = [r for r in archive.infolist() if r.filename == record["entry"]]
                if len(members) != 1 or members[0].file_size != record["bytes"]:
                    raise ValueError("missing/ambiguous/oversized archive member")
                data = verified(archive.read(members[0]), record)
            with (root / record["path"]).open("xb") as target:
                target.write(data)
        archive = work / "tools.7z"
        archive.write_bytes(download(tool_recipe["archive"]))
        for record in tool_recipe["tools"]:
            if record["output_name"] not in ("cmd_Decompiler.exe", "d3dcompiler_47.dll"):
                raise ValueError("unexpected tool output")
            result = subprocess.run([args.seven_zip, "e", "-so", str(archive), record["entry"]],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60, check=True)
            with (tools / record["output_name"]).open("xb") as target:
                target.write(verified(result.stdout, record))
        config = root / "d3dx.ini"
        config.write_bytes(b"[Rendering]\noverride_directory = ShaderFixes\ncache_directory = ShaderCache\n")
        report = inventory.Inventory(config).run()
        inventory_path = work / "inventory.json"
        inventory_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        pins = {r["output_name"]: r["sha256"] for r in tool_recipe["tools"]}
        toolchain = preparer.Toolchain(tools / "cmd_Decompiler.exe", tools / "d3dcompiler_47.dll",
                                      pins["cmd_Decompiler.exe"], pins["d3dcompiler_47.dll"],
                                      args.wine, args.wine_prefix)
        selected = [r["path"] for r in recipe["files"] if r["strict_preparer_expected"] == "pass"]
        if len(selected) != 76:
            raise ValueError("unexpected strict subset")
        prepared = work / "prepared"
        manifest = preparer.prepare(root, inventory_path, prepared, toolchain, selected)
        preparer.check(root, inventory_path, prepared, toolchain)
        outputs = {r["input"]: prepared / r["file"] for r in manifest["outputs"]}
        receipt["strict_subset"] = {"prepared": 76, "verified": True, "recipe_key": manifest["recipe_key"]}
        for record in recipe["files"]:
            if record["path"] not in outputs:
                assembly = work / "excluded-assembly"
                assembly.mkdir()
                original = (root / record["path"]).read_bytes()
                (assembly / "input.asm").write_bytes(original)
                preparer.invoke(toolchain, tools, ["-a", "-S", "input.asm"], assembly, "assemble", assembly)
                code = (assembly / "input.shdr").read_bytes()
                try:
                    preparer.validate_dxbc(code, *preparer.declarations(original))
                except preparer.PreparationError as error:
                    if str(error) != "missing declared input signature register/components":
                        raise
                    receipt["structural_exclusion"] = {"input": record["path"], "reason": str(error)}
                else:
                    raise ValueError("expected structural exclusion was not detected")
                if (assembly / "input.asm").read_bytes() != original:
                    raise ValueError("helper changed staged source")
                outputs[record["path"]] = assembly / "input.shdr"
            path = outputs[record["path"]]
            data = path.read_bytes()
            command = [str(checker), record["stage"], str(path)]
            env = os.environ.copy()
            if args.wine:
                command.insert(0, str(args.wine))
                env.update(WINEPREFIX=str(args.wine_prefix.resolve()), WINEDLLOVERRIDES="d3d11,dxgi=b", WINEDEBUG="-all")
            result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
            text = result.stdout.decode("utf-8")
            match = re.search(r"^(PASS|FAIL) (ps|vs) HRESULT=0x([0-9a-fA-F]+) bytes=(\d+) file=", text, re.M)
            if not match or match[2] != record["stage"] or int(match[4]) != len(data):
                raise ValueError("native checker did not return a complete observation")
            accepted = match[1] == "PASS"
            if result.returncode != (0 if accepted else 1) or path.read_bytes() != data:
                raise ValueError("native checker result/input contract failed")
            receipt["observations"].append({"input": record["path"], "source_sha256": record["sha256"],
                "prepared_sha256": sha(data), "prepared_bytes": len(data), "stage": record["stage"],
                "strict_preparer_expected": record["strict_preparer_expected"],
                "accepted": accepted, "hresult": "0x" + match[3].lower(), "exit_code": result.returncode})
        for record in recipe["files"]:
            verified((root / record["path"]).read_bytes(), record)
        if checker.read_bytes() != checker_bytes:
            raise ValueError("checker changed during collection")
        receipt["inputs_unchanged"] = True
        receipt["accepted"] = sum(r["accepted"] for r in receipt["observations"])
        receipt["rejected"] = len(receipt["observations"]) - receipt["accepted"]
        receipt["all_assets_accepted"] = receipt["rejected"] == 0
        receipt["status"] = "collected"
    return receipt  # Private sources, compiler, bytecode and helper logs have been removed.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checker", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seven-zip", default="7z")
    parser.add_argument("--wine", type=Path)
    parser.add_argument("--wine-prefix", type=Path)
    args = parser.parse_args()
    # Exclusive creation protects an input/existing receipt from replacement.
    with args.output.open("x", encoding="utf-8") as output:
        try:
            receipt = collect(args)
        except Exception as error:
            json.dump({"status": "collection_failed", "error_type": type(error).__name__}, output)
            raise
        json.dump(receipt, output, indent=2)
        output.write("\n")
    print(json.dumps({"status": "collected", "accepted": receipt["accepted"], "rejected": receipt["rejected"],
                      "note": "Collection success is not an asset-validity or rendering claim."}))


if __name__ == "__main__":
    main()
