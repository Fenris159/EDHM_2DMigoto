"""Exercise timeout evidence with the process boundary replaced."""
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location(
    "reload_runner", Path(__file__).with_name("run_reload_notification_tests.py")
)
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class ReloadRunnerTests(unittest.TestCase):
    def test_timeout_retains_partial_streams_and_original_error(self):
        for stdout, stderr, expected_out, expected_err in (
            (b"rendered\n\xff", b"GPU stalled\n", "rendered\n\ufffd", "GPU stalled\n"),
            ("initialized\n", None, "initialized\n", ""),
        ):
            with self.subTest(stdout=stdout), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                dll = root / "product.dll"
                exe = root / "caller.exe"
                dll.write_bytes(b"DLL fixture\n")
                exe.write_bytes(b"process fixture\n")
                output = root / "evidence"
                error = subprocess.TimeoutExpired("fixture", 60, output=stdout, stderr=stderr)
                arguments = ["runner", "--dll", str(dll), "--exe", str(exe), "--output", str(output)]
                with patch("sys.argv", arguments), patch.object(runner.subprocess, "run", side_effect=error):
                    with self.assertRaises(subprocess.TimeoutExpired) as raised:
                        runner.main()
                self.assertIs(raised.exception, error)
                case = output / "default"
                self.assertEqual(sorted(p.name for p in case.glob("*.txt")), ["stderr.txt", "stdout.txt"])
                self.assertEqual((case / "stdout.txt").read_text(encoding="utf-8"), expected_out)
                self.assertEqual((case / "stderr.txt").read_text(encoding="utf-8"), expected_err)
                self.assertEqual(json.loads((output / "results.json").read_text())["cases"],
                                 [{"case": "default", "passed": False, "timeout": True}])


if __name__ == "__main__":
    unittest.main()
