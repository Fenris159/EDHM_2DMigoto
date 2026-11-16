"""Observe actual command output when a third-party DLL suppresses D3D11 Present."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("dll", "exe", "fixture", "output"):
        parser.add_argument(f"--{name}", required=True, type=Path)
    parser.add_argument("--runner", nargs="*", default=[])
    parser.add_argument("--hardware", action="store_true")
    parser.add_argument("--expect-starvation", action="store_true")
    args = parser.parse_args()
    inputs = {name: getattr(args, name).resolve(strict=True) for name in ("dll", "exe", "fixture")}
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=False)
    for name, target in (("dll", "d3d11.dll"), ("exe", "ProxyFrameTests.exe"),
                         ("fixture", "FrameProxyFixture.dll")):
        shutil.copy2(inputs[name], directory / target)
    (directory / "d3dx.ini").write_text(
        "[Logging]\nenabled=1\ncalls=0\nunbuffered=1\nshow_warnings=0\n"
        "[System]\nload_library_redirect=0\nallow_platform_update=1\n"
        "[Hunting]\nhunting=0\n[Rendering]\nstereo_params=-1\nini_params=120\n"
        "[Constants]\nx=0\ny=0\n[Present]\nx=x+1\npost y=y+1\n", encoding="utf-8")
    command = args.runner + [str(directory / "ProxyFrameTests.exe")]
    if args.hardware:
        command.append("hardware")
    record = {"passed": False, "expected_starvation": args.expect_starvation,
              "inputs": {name: {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                         for name, path in inputs.items()}}
    try:
        try:
            result = subprocess.run(command, cwd=directory, capture_output=True, text=True, timeout=60, check=False)
        except subprocess.TimeoutExpired as error:
            record["timeout"] = True
            for name, captured in (("stdout", error.stdout), ("stderr", error.stderr)):
                text = captured.decode("utf-8", errors="replace") if isinstance(captured, bytes) else captured or ""
                (directory / f"{name}.txt").write_text(text, encoding="utf-8")
            raise
        record["exit_code"] = result.returncode
        (directory / "stdout.txt").write_text(result.stdout, encoding="utf-8")
        (directory / "stderr.txt").write_text(result.stderr, encoding="utf-8")
        if args.expect_starvation:
            record["passed"] = (result.returncode == 1 and "commands x=0 y=0 expected 1 1" in result.stdout
                                and "Present commands were starved or duplicated" in result.stderr)
        else:
            record["passed"] = result.returncode == 0 and "PASS proxy frame actions" in result.stdout
        print(result.stdout, end="", flush=True)
        if not record["passed"]:
            raise RuntimeError(result.stderr or "proxy frame check failed")
    finally:
        (directory / "results.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
