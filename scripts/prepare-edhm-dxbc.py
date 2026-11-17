#!/usr/bin/env python3
"""Prepare copied SM5 pixel/vertex assembly with a pinned upstream helper."""

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import struct
import subprocess
import tempfile
from typing import Optional


class PreparationError(RuntimeError):
    pass


@dataclass(frozen=True)
class Toolchain:
    assembler: Path
    compiler: Path
    assembler_sha256: str
    compiler_sha256: str
    wine: Optional[Path] = None
    wine_prefix: Optional[Path] = None


ASSET = re.compile(r"^[0-9a-f]{16}-(ps|vs)\.txt$", re.I)
MODEL = re.compile(r"^\s*((?:ps|vs)_5_0)\s*$", re.M | re.I)
DECLARATION = re.compile(r"^\s*dcl_(input|output)\w*\b.*?\b([vo])(\d+)(?:\.([xyzw]+))?\b", re.I)
PROTOCOL = "3dmigoto-cmd-assembly-v1"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def decode(data):
    return data.decode("utf-16" if data.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig")


def pe_machine(data):
    if len(data) < 64 or data[:2] != b"MZ":
        raise PreparationError("tool is not a PE image")
    offset = struct.unpack_from("<I", data, 60)[0]
    if offset < 64 or offset + 26 > len(data) or data[offset:offset + 4] != b"PE\0\0":
        raise PreparationError("invalid PE header")
    machine = struct.unpack_from("<H", data, offset + 4)[0]
    magic = struct.unpack_from("<H", data, offset + 24)[0]
    if (machine, magic) not in ((0x14C, 0x10B), (0x8664, 0x20B)):
        raise PreparationError("unsupported tool architecture")
    return machine


def tool_inputs(tools):
    records, binaries, machines = {}, {}, []
    for name, path, expected in (("cmd_Decompiler.exe", tools.assembler, tools.assembler_sha256),
                                 ("d3dcompiler_47.dll", tools.compiler, tools.compiler_sha256)):
        data = path.read_bytes()
        if not re.fullmatch(r"[0-9a-fA-F]{64}", expected) or digest(data) != expected.lower():
            raise PreparationError("tool digest mismatch: " + name)
        machines.append(pe_machine(data))
        records[name] = {"sha256": digest(data), "bytes": len(data)}
        binaries[name] = data
    if machines[0] != machines[1]:
        raise PreparationError("assembler/compiler architecture mismatch")
    if (tools.wine is None) != (tools.wine_prefix is None):
        raise PreparationError("Wine requires an explicit isolated prefix")
    records.update(machine=hex(machines[0]), protocol=PROTOCOL)
    return records, binaries


def declarations(data):
    text = decode(data)
    models = MODEL.findall(text)
    if len(models) != 1:
        raise PreparationError("expected one supported ps_5_0 or vs_5_0 model")
    declared = {"input": {}, "output": {}}
    for kind in declared:
        if not re.search(r"^\s*//\s*" + kind + r" signature:", text, re.M | re.I):
            raise PreparationError("missing required " + kind + " signature comments")
    for line in text.splitlines():
        code = line.split("//", 1)[0]
        if not re.match(r"\s*dcl_(input|output)", code, re.I):
            continue
        match = DECLARATION.match(code)
        if not match or match[2].lower() != ("v" if match[1].lower() == "input" else "o"):
            raise PreparationError("unsupported input/output declaration")
        mask = sum(1 << "xyzw".index(component) for component in (match[4] or "xyzw"))
        kind, register = match[1].lower(), int(match[3])
        declared[kind][register] = declared[kind].get(register, 0) | mask
    return models[0].lower(), declared


def validate_dxbc(data, model, declared):
    if len(data) < 32 or data[:4] != b"DXBC":
        raise PreparationError("malformed DXBC header")
    version, length, count = struct.unpack_from("<III", data, 20)
    table_end = 32 + count * 4
    if version != 1 or length != len(data) or not 1 <= count <= 16 or table_end > length:
        raise PreparationError("malformed DXBC extent/table")
    chunks, ranges = {}, []
    for index in range(count):
        offset = struct.unpack_from("<I", data, 32 + index * 4)[0]
        if offset < table_end or offset + 8 > length:
            raise PreparationError("malformed DXBC chunk offset")
        name, size = struct.unpack_from("<4sI", data, offset)
        end = offset + 8 + size
        if end > length or name in chunks or any(offset < right and left < end for left, right in ranges):
            raise PreparationError("malformed DXBC chunk extent/overlap")
        chunks[name] = data[offset + 8:end]
        ranges.append((offset, end))
    code_chunks = [chunks[name] for name in (b"SHDR", b"SHEX") if name in chunks]
    if len(code_chunks) != 1 or len(code_chunks[0]) < 8 or len(code_chunks[0]) % 4:
        raise PreparationError("missing/invalid shader code")
    token, words = struct.unpack_from("<II", code_chunks[0])
    if token != (0x50 if model == "ps_5_0" else 0x10050) or words * 4 != len(code_chunks[0]):
        raise PreparationError("shader stage/model or code extent mismatch")
    signatures = {}
    for kind, name in (("input", b"ISGN"), ("output", b"OSGN")):
        payload = chunks.get(name, b"")
        if len(payload) < 8:
            raise PreparationError("missing required " + kind + " signature")
        entries, start = struct.unpack_from("<II", payload)
        if start != 8 or entries > 128 or 8 + entries * 24 > len(payload):
            raise PreparationError("invalid signature table")
        registers = {}
        for index in range(entries):
            entry = 8 + index * 24
            semantic = struct.unpack_from("<I", payload, entry)[0]
            register = struct.unpack_from("<I", payload, entry + 16)[0]
            mask = payload[entry + 20]
            if semantic < 8 + entries * 24 or semantic >= len(payload) or b"\0" not in payload[semantic:] or mask & ~15:
                raise PreparationError("invalid signature entry")
            registers[register] = registers.get(register, 0) | mask
        if any(registers.get(reg, 0) & mask != mask for reg, mask in declared[kind].items()):
            raise PreparationError("missing declared " + kind + " signature register/components")
        signatures[kind] = entries
    return {"model": model, "signature_entries": signatures}


def snapshot(root, inventory_path, tools, selected=()):
    inventory_bytes = inventory_path.read_bytes()
    inventory = json.loads(inventory_bytes)
    if inventory.get("schema_version") != 1 or not isinstance(inventory.get("files"), list) or len(inventory["files"]) > 10000:
        raise PreparationError("unsupported inventory schema/extent")
    inputs, assets, source_bytes = [], [], {}
    wanted = set(selected)
    for record in inventory["files"]:
        path = PurePosixPath(record["path"])
        if path.is_absolute() or ".." in path.parts or "\\" in str(path) or ":" in str(path):
            raise PreparationError("inventory path needs a prepared local layout")
        label = path.as_posix()
        if label in source_bytes:
            raise PreparationError("duplicate inventory path")
        data = (root / label).read_bytes()
        if digest(data) != record["sha256"] or len(data) != record["bytes"]:
            raise PreparationError("input digest/size mismatch: " + label)
        inputs.append({"path": label, "sha256": digest(data), "bytes": len(data)})
        source_bytes[label] = data
        if record.get("role") != "replacement" or (wanted and label not in wanted):
            continue
        match = ASSET.fullmatch(path.name)
        if record.get("kind") != "assembly" or not match:
            raise PreparationError("only signature-bearing SM5 pixel/vertex assembly is supported")
        model, declared = declarations(data)
        if model[:2] != match[1].lower():
            raise PreparationError("filename stage disagrees with assembly model")
        assets.append({"path": label, "model": model, "declared": declared})
    if not assets or (wanted and {a["path"] for a in assets} != wanted):
        raise PreparationError("selected assembly assets are missing")
    names = [PurePosixPath(a["path"]).stem.lower() for a in assets]
    if len(set(names)) != len(names):
        raise PreparationError("ambiguous output names")
    tool_record, binaries = tool_inputs(tools)
    recipe = {"protocol": PROTOCOL, "inventory_sha256": digest(inventory_bytes),
              "inputs": inputs, "assets": assets, "tools": tool_record}
    # JSON serialisation normalises integer register keys for later checks.
    recipe = json.loads(json.dumps(recipe))
    key = digest(json.dumps(recipe, sort_keys=True, separators=(",", ":")).encode())
    return recipe, key, source_bytes, binaries


def invoke(tools, tool_directory, arguments, directory, action, logs):
    command = [str(tool_directory / "cmd_Decompiler.exe")] + arguments
    env = os.environ.copy()
    if tools.wine:
        command.insert(0, str(tools.wine))
        env.update(WINEPREFIX=str(tools.wine_prefix.resolve()), WINEDLLOVERRIDES="d3dcompiler_47=n")
    try:
        result = subprocess.run(command, cwd=directory, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=60)
    except subprocess.TimeoutExpired as error:
        (logs / (action + ".log")).write_bytes(error.stdout or b"")
        raise PreparationError("helper timed out: " + action) from error
    (logs / (action + ".log")).write_bytes(result.stdout)
    if result.returncode != 0:
        raise PreparationError("helper failed: " + action + " (exit " + str(result.returncode) + ")")
    if action == "roundtrip" and b"Assembly verification pass succeeded" not in result.stdout:
        raise PreparationError("helper did not confirm roundtrip verification")


def prepare(root, inventory_path, output, tools, selected=()):
    root, inventory_path, output = root.resolve(), inventory_path.resolve(), output.absolute()
    if output.exists() or output.is_symlink():
        raise PreparationError("output must be a new directory")
    if tools.wine is None and os.name != "nt":
        raise PreparationError("native PE execution requires Windows; supply Wine and an isolated prefix")
    recipe, key, sources, binaries = snapshot(root, inventory_path, tools, selected)
    output.mkdir()
    marker = output / ".incomplete"
    marker.write_text("No successful preparation claim while this file exists.\n")
    try:
        (output / "shaders").mkdir()
        outputs = []
        with tempfile.TemporaryDirectory(prefix=".edhm-dxbc-", dir=output.parent) as temporary:
            work = Path(temporary)
            tool_directory = work / "tools"
            tool_directory.mkdir()
            for name, data in binaries.items():
                (tool_directory / name).write_bytes(data)
            for index, asset in enumerate(recipe["assets"]):
                assembly, validation = work / str(index) / "assembly", work / str(index) / "validation"
                assembly.mkdir(parents=True)
                validation.mkdir()
                logs = output / "logs" / str(index)
                logs.mkdir(parents=True)
                (assembly / "input.asm").write_bytes(sources[asset["path"]])
                invoke(tools, tool_directory, ["-a", "-S", "input.asm"], assembly, "assemble", logs)
                code = (assembly / "input.shdr").read_bytes()
                model, declared = declarations(sources[asset["path"]])
                summary = validate_dxbc(code, model, declared)
                (validation / "input.shdr").write_bytes(code)
                invoke(tools, tool_directory, ["-d", "-V", "-S", "input.shdr"], validation, "roundtrip", logs)
                invoke(tools, tool_directory, ["--disassemble-ms", "-S", "input.shdr"], validation, "microsoft", logs)
                disassembly = decode((validation / "input.msasm").read_bytes())
                if not re.search(r"^\s*" + model + r"\s*$", disassembly, re.M):
                    raise PreparationError("Microsoft disassembly did not confirm the model")
                if (assembly / "input.asm").read_bytes() != sources[asset["path"]] or (validation / "input.shdr").read_bytes() != code:
                    raise PreparationError("helper changed a staged input")
                filename = "shaders/" + PurePosixPath(asset["path"]).stem.lower() + ".dxbc"
                with (output / filename).open("xb") as target:
                    target.write(code)
                outputs.append({"input": asset["path"], "file": filename, "sha256": digest(code),
                                "bytes": len(code), "structure": summary,
                                "original_identity": {"status": "unverified", "original_container_sha256": None, "game_build": None}})
        _, final_key, _, _ = snapshot(root, inventory_path, tools, selected)
        if final_key != key:
            raise PreparationError("inputs or tools changed during preparation")
        manifest = {"schema_version": 1, "kind": "edhm-prepared-dxbc", "status": "complete",
                    "execution": "wine-native-compiler" if tools.wine else "windows-native-compiler",
                    "structural_only": True, "recipe_key": key, "recipe": recipe, "outputs": outputs}
        temporary_manifest = output / ".manifest.tmp"
        temporary_manifest.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        temporary_manifest.rename(output / "manifest.json")
        marker.unlink()
        return manifest
    except Exception as error:
        try:
            with (output / "failure.json").open("x", encoding="utf-8") as failure:
                json.dump({"status": "incomplete", "error": str(error)}, failure)
        except OSError:
            pass  # Preserve the original error if evidence cannot be written.
        raise


def check(root, inventory_path, output, tools):
    if (output / ".incomplete").exists():
        raise PreparationError("preparation is incomplete")
    manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
    if (manifest.get("schema_version"), manifest.get("kind"), manifest.get("status"), manifest.get("structural_only")) != (1, "edhm-prepared-dxbc", "complete", True):
        raise PreparationError("unsupported or incomplete manifest")
    selected = [asset["path"] for asset in manifest["recipe"]["assets"]]
    recipe, key, sources, _ = snapshot(root.resolve(), inventory_path.resolve(), tools, selected)
    if key != manifest["recipe_key"] or recipe != manifest["recipe"] or len(manifest["outputs"]) != len(selected):
        raise PreparationError("preparation recipe is stale")
    for asset, record in zip(recipe["assets"], manifest["outputs"]):
        expected = "shaders/" + PurePosixPath(asset["path"]).stem.lower() + ".dxbc"
        if record["input"] != asset["path"] or record["file"] != expected:
            raise PreparationError("output identity mismatch")
        data = (output / expected).read_bytes()
        if digest(data) != record["sha256"] or len(data) != record["bytes"]:
            raise PreparationError("prepared output digest/size mismatch")
        validate_dxbc(data, *declarations(sources[asset["path"]]))
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "check"))
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--assembler", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--assembler-sha256", required=True)
    parser.add_argument("--compiler-sha256", required=True)
    parser.add_argument("--wine", type=Path)
    parser.add_argument("--wine-prefix", type=Path)
    parser.add_argument("--asset", action="append", default=[], help="Inventory-relative assembly path; repeat to select a subset")
    args = parser.parse_args()
    tools = Toolchain(args.assembler, args.compiler, args.assembler_sha256, args.compiler_sha256,
                      args.wine, args.wine_prefix)
    try:
        if args.action == "prepare":
            manifest = prepare(args.root, args.inventory, args.output, tools, args.asset)
        else:
            if args.asset:
                parser.error("check uses the manifest's selected assets")
            manifest = check(args.root, args.inventory, args.output, tools)
        print(json.dumps({"status": "complete", "outputs": len(manifest["outputs"]), "recipe_key": manifest["recipe_key"]}))
    except (OSError, ValueError, KeyError, TypeError, PreparationError) as error:
        parser.exit(1, "DXBC preparation failed: " + str(error) + "\n")


if __name__ == "__main__":
    main()
