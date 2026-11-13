"""Exercise mutation-runner cleanup with the compiler/process boundary replaced."""

import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location(
    "mutation_runner", Path(__file__).with_name("check_mutations.py")
)
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class MutationCleanupTests(unittest.TestCase):
    def exercise_failure(self, mode):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "DirectX11").mkdir()
            original = (
                b'\tif (tree->tokens.empty())\n'
                b'\t\tthrow CommandListSyntaxError(L"Expression inside parentheses must not be empty", 0);'
            )
            source = root / "DirectX11" / "CommandList.cpp"
            source.write_bytes(original)
            settings = root / "DirectX11" / "IniHandler.cpp"
            settings.write_bytes(b"settings fixture\r\n")
            profiling = root / "DirectX11" / "profiling.cpp"
            profiling.write_bytes(b"profiling fixture\r\n")
            executable = root / "RuntimeTests.exe"
            executable.write_bytes(b"production\n")
            hashing = root / "DirectX11" / "ResourceHash.cpp"
            hashing.write_bytes(b"hashing fixture\r\n")
            originals = {p: p.read_bytes() for p in (source, settings, profiling, hashing)}
            mutant_built = False

            def process_boundary(command, **kwargs):
                nonlocal mutant_built
                if command == ["fixture-compiler"]:
                    if source.read_bytes() != original:
                        mutant_built = True
                        executable.write_bytes(b"mutant\n")
                        return subprocess.CompletedProcess(command, 7 if mode == "build-fails" else 0, b"", b"")
                    if mutant_built and mode == "restoration-fails":
                        executable.write_bytes(b"incomplete\n")
                        return subprocess.CompletedProcess(command, 8, b"", b"")
                    executable.write_bytes(b"production\n")
                    return subprocess.CompletedProcess(command, 0, b"", b"")
                return subprocess.CompletedProcess(command, 0, f"RUN {command[1]}\n".encode(), b"")

            expected_error = (
                "Mutation must compile successfully: empty-groups"
                if mode == "build-fails"
                else "Regression check did not detect mutation: empty-groups"
            )
            with (
                patch.object(runner, "ROOT", root),
                patch.object(runner, "OUTPUT", root / "evidence"),
                patch.object(runner, "EXE", executable),
                patch.object(runner, "BUILD", ["fixture-compiler"]),
                patch.object(runner.subprocess, "run", side_effect=process_boundary),
            ):
                with self.assertRaisesRegex(RuntimeError, expected_error):
                    runner.main()
            self.assertTrue(mutant_built, "the mutation failure was never reached")
            for path, content in originals.items():
                self.assertEqual(path.read_bytes(), content, "cleanup changed original source bytes")
            if mode == "restoration-fails":
                self.assertFalse(executable.exists(), "a failed restoration left an untrusted executable")
            else:
                self.assertEqual(executable.read_bytes(), b"production\n", "cleanup left the mutant executable")

    def test_undetected_mutation_restores_production_binary(self):
        self.exercise_failure("unexpected-pass")

    def test_failed_mutant_build_restores_production_binary(self):
        self.exercise_failure("build-fails")

    def test_failed_restoration_quarantines_binary_and_preserves_original_error(self):
        self.exercise_failure("restoration-fails")


if __name__ == "__main__":
    unittest.main()
