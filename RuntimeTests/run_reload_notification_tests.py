"""Verify reload confirmation pixels, constants, warning independence and disk logging."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def configuration(index, value, quiet, warnings):
    setting = "show_reload_message=0\n" if quiet else ""
    return ("[Logging]\nenabled=1\ncalls=0\ndebug=0\nunbuffered=1\n"
            f"show_warnings={int(warnings)}\n{setting}"
            "[System]\nload_library_redirect=0\nallow_platform_update=1\n"
            "[Hunting]\nhunting=0\n[Rendering]\nstereo_params=-1\nini_params=120\n"
            f"[Constants]\nx={value}\nx{index}=1\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dll", required=True, type=Path)
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--runner", nargs="*", default=[])
    parser.add_argument("--hardware", action="store_true")
    parser.add_argument("--expect-visible", action="store_true",
                        help="Old-DLL control: quiet cases must fail specifically on visible confirmation pixels")
    args = parser.parse_args()
    inputs = {name: getattr(args, name).resolve(strict=True) for name in ("dll", "exe")}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    records = []
    try:
        for case in ("default", "quiet", "warnings"):
            directory = output / case
            directory.mkdir()
            shutil.copy2(inputs["dll"], directory / "d3d11.dll")
            shutil.copy2(inputs["exe"], directory / "ReloadNotificationTests.exe")
            # The quiet setting first appears during reload; the new value and
            # extent must reach the GPU even when confirmation pixels disappear.
            (directory / "d3dx.ini").write_text(configuration(300, 1, False, False), encoding="utf-8")
            (directory / "reload.ini").write_text(
                configuration(301, 7, case != "default", case == "warnings"), encoding="utf-8")
            command = args.runner + [str(directory / "ReloadNotificationTests.exe"), case]
            if args.hardware:
                command.append("hardware")
            record = {"case": case, "passed": False}
            records.append(record)
            result = subprocess.run(command, cwd=directory, capture_output=True, text=True, timeout=60, check=False)
            (directory / "stdout.txt").write_text(result.stdout, encoding="utf-8")
            (directory / "stderr.txt").write_text(result.stderr, encoding="utf-8")
            record["exit_code"] = result.returncode
            log = (directory / "d3d11_log.txt").read_text(encoding="utf-8", errors="replace")
            record["reload_log_count"] = log.count("> Reloaded config in")
            if args.expect_visible and case != "default":
                record["passed"] = (result.returncode == 1 and record["reload_log_count"] == 1
                                    and "suppressed reload notification remained visible" in result.stderr)
            else:
                record["passed"] = (result.returncode == 0 and record["reload_log_count"] == 1
                                    and f"PASS reload notification {case}" in result.stdout)
            print(f"{'PASS' if record['passed'] else 'FAIL'} reload notification {case}", flush=True)
            if not record["passed"]:
                raise RuntimeError(f"{case}: {result.stdout}\n{result.stderr}")
    finally:
        (output / "results.json").write_text(json.dumps({
            "inputs": {name: {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                       for name, path in inputs.items()},
            "expected_visible_control": args.expect_visible,
            "cases": records,
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
