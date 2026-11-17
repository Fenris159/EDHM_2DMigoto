"""Offline preparer outcomes; only the third-party process is substituted."""

import importlib.util
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).parents[1] / "scripts/prepare-edhm-dxbc.py"
SPEC = importlib.util.spec_from_file_location("dxbc_preparer", SCRIPT)
preparer = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = preparer
SPEC.loader.exec_module(preparer)
FIXTURES = Path(__file__).parent / "fixtures"
# This file is a template: canonical LF bytes keep test inputs fixed on Windows.
SOURCE = (FIXTURES / "preparer-minimal-ps.asm").read_text(encoding="utf-8").encode("utf-8")
DXBC = bytes.fromhex((FIXTURES / "preparer-minimal-ps.dxbc.hex").read_text())
SOURCE_SHA = "43f30d79d98bd13922e2d38e07d94a96600ec93cf3e9a78f0f406aa7315b8ff7"
DXBC_SHA = "bf498b34c6ce22e0f77e6ad9502c0204a134309edff5e39184a8a30eab6579a8"
TOOL_SHA = "3069cb325abda70f85a92b4227480b9d98e5482bd9b855d6884fd9060d0502fa"
UPDATED_TOOL_SHA = "5cfee2410b8bb71af0d9353366b77418954067c1f7ebbbc25e8f0d730a49013b"
X64_TOOL_SHA = "41ea1679608cb790a9f2916e774b582afc9aac15737de7d71e79538f43aca0f3"
ASSET = "ShaderFixes/0123456789abcdef-ps.txt"


def pe_image(machine=0x14C, magic=0x10B):
    data = bytearray(128)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 60, 64)
    data[64:68] = b"PE\0\0"
    struct.pack_into("<H", data, 68, machine)
    struct.pack_into("<H", data, 84, 2)
    struct.pack_into("<H", data, 88, magic)
    return bytes(data)


class PreparerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "ShaderFixes").mkdir()
        self.asset = self.root / ASSET
        self.asset.write_bytes(SOURCE)
        self.inventory = self.root / "inventory.json"
        self.write_inventory(SOURCE)
        self.assembler = self.root / "helper.exe"
        self.compiler = self.root / "d3dcompiler_47.dll"
        self.assembler.write_bytes(pe_image())
        self.compiler.write_bytes(pe_image())
        self.tools = preparer.Toolchain(self.assembler, self.compiler, TOOL_SHA, TOOL_SHA,
                                       Path("fixture-wine"), self.root / "prefix")
        self.output = self.root / "prepared"

    def write_inventory(self, data):
        # These are request inputs; result digests below use frozen literals.
        self.inventory.write_text(json.dumps({"schema_version": 1, "files": [
            {"path": ASSET, "role": "replacement", "kind": "assembly",
             "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        ]}))

    def boundary(self, code=DXBC, fail=None):
        def run(command, *, cwd, env, stdout, stderr, timeout):
            del env, stdout, stderr, timeout
            directory = Path(cwd)
            action = "assemble" if "-a" in command else "roundtrip" if "-V" in command else "microsoft"
            if action == "assemble":
                (directory / "input.shdr").write_bytes(code)
            elif action == "roundtrip":
                # Real cmd_Decompiler overwrites same-basename assembly here.
                (directory / "input.asm").write_bytes(b"disassembled text, not the immutable input\n")
            else:
                (directory / "input.msasm").write_bytes(b"ps_5_0\n")
            if action == fail:
                return subprocess.CompletedProcess(command, 1, stdout=b"parse or validation error\n")
            return subprocess.CompletedProcess(command, 0, stdout=b"Assembly verification pass succeeded\n")
        return patch.object(preparer.subprocess, "run", side_effect=run)

    def test_prepares_verified_bytes_without_changing_inputs(self):
        original = {p: p.read_bytes() for p in (self.asset, self.inventory, self.assembler, self.compiler)}
        with self.boundary():
            result = preparer.prepare(self.root, self.inventory, self.output, self.tools)
        self.assertEqual(result["status"], "complete")
        self.assertEqual(result["recipe"]["inputs"], [{"path": ASSET, "bytes": 522, "sha256": SOURCE_SHA}])
        self.assertEqual(result["outputs"][0]["sha256"], DXBC_SHA)
        self.assertEqual(result["outputs"][0]["bytes"], 212)
        self.assertEqual(result["outputs"][0]["structure"],
                         {"model": "ps_5_0", "signature_entries": {"input": 1, "output": 1}})
        self.assertEqual(result["outputs"][0]["original_identity"],
                         {"status": "unverified", "original_container_sha256": None, "game_build": None})
        self.assertEqual((self.output / "shaders/0123456789abcdef-ps.dxbc").read_bytes(), DXBC)
        self.assertEqual({p: p.read_bytes() for p in original}, original)
        self.assertFalse((self.output / ".incomplete").exists())
        verifier = preparer.Toolchain(self.assembler, self.compiler, TOOL_SHA, TOOL_SHA)
        self.assertEqual(preparer.check(self.root, self.inventory, self.output, verifier), result)
        self.assertEqual(sorted(p.name for p in self.output.iterdir()), ["logs", "manifest.json", "shaders"])

    def test_changed_assets_and_tools_invalidate_prepared_results(self):
        with self.boundary():
            first = preparer.prepare(self.root, self.inventory, self.output, self.tools)
        self.asset.write_bytes(SOURCE + b"// changed\n")
        with self.assertRaisesRegex(preparer.PreparationError, "input digest"):
            preparer.check(self.root, self.inventory, self.output, self.tools)
        self.write_inventory(self.asset.read_bytes())
        with self.assertRaisesRegex(preparer.PreparationError, "recipe is stale"):
            preparer.check(self.root, self.inventory, self.output, self.tools)
        self.asset.write_bytes(SOURCE)
        self.write_inventory(SOURCE)
        self.assembler.write_bytes(pe_image() + b"x")
        with self.assertRaisesRegex(preparer.PreparationError, "tool digest"):
            preparer.check(self.root, self.inventory, self.output, self.tools)
        updated = preparer.Toolchain(self.assembler, self.compiler, UPDATED_TOOL_SHA, TOOL_SHA,
                                     Path("fixture-wine"), self.root / "prefix")
        with self.assertRaisesRegex(preparer.PreparationError, "recipe is stale"):
            preparer.check(self.root, self.inventory, self.output, updated)
        with self.boundary():
            second = preparer.prepare(self.root, self.inventory, self.root / "new-prepared", updated)
        self.assertNotEqual(first["recipe_key"], second["recipe_key"])

    def test_architecture_mismatch_cannot_publish_a_manifest(self):
        self.compiler.write_bytes(pe_image(0x8664, 0x20B))
        tools = preparer.Toolchain(self.assembler, self.compiler, TOOL_SHA, X64_TOOL_SHA,
                                   Path("fixture-wine"), self.root / "prefix")
        with self.assertRaisesRegex(preparer.PreparationError, "architecture mismatch"):
            preparer.prepare(self.root, self.inventory, self.output, tools)
        self.assertFalse((self.output / "manifest.json").exists())

    def test_helper_errors_never_publish_success(self):
        for action in ("assemble", "roundtrip", "microsoft"):
            with self.subTest(action=action), self.boundary(fail=action):
                output = self.root / action
                with self.assertRaisesRegex(preparer.PreparationError, "helper failed"):
                    preparer.prepare(self.root, self.inventory, output, self.tools)
                self.assertFalse((output / "manifest.json").exists())
                self.assertTrue((output / ".incomplete").exists())

    def test_missing_source_signatures_cannot_publish_success(self):
        source = SOURCE.replace(b"// Input signature:", b"// missing signature:")
        self.asset.write_bytes(source)
        self.write_inventory(source)
        with self.assertRaisesRegex(preparer.PreparationError, "missing required input signature"):
            preparer.prepare(self.root, self.inventory, self.output, self.tools)
        self.assertFalse((self.output / "manifest.json").exists())

    def test_malformed_output_or_missing_declared_signature_cannot_publish_success(self):
        missing_register = bytearray(DXBC)
        struct.pack_into("<I", missing_register, 76, 1)  # ISGN register must cover declared v0.x.
        for index, code in enumerate((b"DXBC", DXBC[:-4], bytes(missing_register))):
            with self.subTest(index=index), self.boundary(code=code):
                output = self.root / ("bad-output-" + str(index))
                with self.assertRaises(preparer.PreparationError):
                    preparer.prepare(self.root, self.inventory, output, self.tools)
                self.assertFalse((output / "manifest.json").exists())

    def test_existing_outputs_and_inputs_are_never_overwritten(self):
        self.output.mkdir()
        sentinel = self.output / "manifest.json"
        sentinel.write_bytes(b"keep existing output\n")
        for output in (self.output, self.asset):
            with self.subTest(output=output), self.assertRaisesRegex(preparer.PreparationError, "new directory"):
                preparer.prepare(self.root, self.inventory, output, self.tools)
        self.assertEqual(sentinel.read_bytes(), b"keep existing output\n")
        self.assertEqual(self.asset.read_bytes(), SOURCE)

    def test_changed_prepared_bytes_are_rejected(self):
        with self.boundary():
            preparer.prepare(self.root, self.inventory, self.output, self.tools)
        changed = bytearray(DXBC)
        changed[4] ^= 1  # A changed digest field leaves the structural tables intact.
        (self.output / "shaders/0123456789abcdef-ps.dxbc").write_bytes(changed)
        with self.assertRaisesRegex(preparer.PreparationError, "prepared output digest"):
            preparer.check(self.root, self.inventory, self.output, self.tools)


if __name__ == "__main__":
    unittest.main()
