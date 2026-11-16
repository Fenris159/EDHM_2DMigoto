"""Observable passive-inventory checks with independent on-disk fixtures."""

import importlib.util
from pathlib import Path
import tempfile
import unittest


SPEC = importlib.util.spec_from_file_location(
    "readiness_inventory", Path(__file__).parents[1] / "scripts" / "inventory-edhm.py"
)
inventory = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(inventory)


class ReadinessInventoryTests(unittest.TestCase):
    def layout(self, directory, contents):
        root = Path(directory)
        (root / "ShaderFixes").mkdir()
        (root / "ShaderCache").mkdir()
        for name, data in contents.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        return root

    def test_recursive_exclusions_and_include_cycles_terminate(self):
        contents = {
            "d3dx.ini": b"[Include]\ninclude_recursive = mods\nexclude_recursive = DISABLED*\n",
            "mods/a.ini": b"[Include]\ninclude = ../d3dx.ini\n",
            "mods/b.ini": b"[Constants]\nx1 = 7\n",
            "mods/nested/c.ini": b"condition = 0\n[ResourceExcluded]\nfilename = missing.dds\n",
            "mods/disabled.ini": b"[ResourceWrong]\n",
            "mods/DisabledPack/hidden.ini": b"[ResourceWrong]\n",
        }
        with tempfile.TemporaryDirectory() as directory:
            root = self.layout(directory, contents)
            report = inventory.Inventory(root / "d3dx.ini").run()
            self.assertEqual(report["summary"]["files_by_role"], {"include": 3, "root": 1})
            self.assertEqual(report["summary"]["sections"], {"constants": 1, "include": 2})
            self.assertCountEqual(
                [(d["path"], d["code"]) for d in report["diagnostics"]],
                [("mods/DisabledPack", "excluded_recursive"),
                 ("mods/disabled.ini", "excluded_recursive"),
                 ("d3dx.ini", "already_seen")],
            )
            excluded = next(f for f in report["files"] if f["path"] == "mods/nested/c.ini")
            self.assertEqual(excluded["activation"], "excluded_literal_zero")

    def test_exact_asset_digest_and_all_input_bytes_survive_inventory(self):
        contents = {
            "d3dx.ini": b"[Rendering]\noverride_directory = ShaderFixes\n",
            "ShaderFixes/0123456789abcdef-ps.txt": b"abc",
        }
        with tempfile.TemporaryDirectory() as directory:
            root = self.layout(directory, contents)
            report = inventory.Inventory(root / "d3dx.ini").run()
            shader = next(f for f in report["files"] if f["role"] == "replacement")
            self.assertEqual(shader["sha256"], "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
            self.assertEqual(shader["bytes"], 3)
            self.assertEqual({name: (root / name).read_bytes() for name in contents}, contents)
            self.assertEqual(sorted(p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file()), sorted(contents))

    def test_filename_and_cached_binary_do_not_establish_original_identity(self):
        contents = {
            "d3dx.ini": b"[Rendering]\nshader_hash = 3dmigoto\n",
            "ShaderFixes/0123456789abcdef-ps.txt": b"ps_5_0\n// Input signature:\n// Output signature:\n",
            "ShaderCache/0123456789abcdef-ps.bin": b"DXBC",
        }
        with tempfile.TemporaryDirectory() as directory:
            root = self.layout(directory, contents)
            report = inventory.Inventory(root / "d3dx.ini").run()
            shaders = [f for f in report["files"] if "filename_key" in f]
            self.assertEqual([s["kind"] for s in shaders], ["assembly", "binary_candidate"])
            for shader in shaders:
                self.assertEqual(shader["filename_key"], "0123456789abcdef")
                self.assertEqual(shader["stage"], "ps")
                self.assertEqual(shader["original_identity"], {"status": "unverified", "original_container_sha256": None, "game_build": None})
            self.assertEqual(shaders[0]["declared_shader_model"], "ps_5_0")
            self.assertEqual(shaders[0]["signature_comments"], {"Input": True, "Output": True, "Patch Constant": False})
            self.assertFalse(report["complete_runtime_equivalence"])


if __name__ == "__main__":
    unittest.main()
