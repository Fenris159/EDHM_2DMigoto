"""Exercise the built product DLL across a third-party D3D11 proxy boundary."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dll", required=True, type=Path)
    parser.add_argument("--fixture", required=True, type=Path)
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--runner", default="")
    parser.add_argument("--system-root", type=Path)
    parser.add_argument("--edup", type=Path)
    parser.add_argument("--expect-bypass", action="store_true")
    args = parser.parse_args()
    inputs = {name: getattr(args, name).resolve(strict=True) for name in ("dll", "fixture", "exe")}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    cases = ["chain-lazy", "chain-early", "chain-swap-early"]
    if not args.expect_bypass:
        cases += ["standalone", "downstream", "missing-proxy", "self-proxy", "invalid-proxy"]
        if args.edup:
            cases += ["edup"]
    records = []
    try:
        for case in cases:
            directory = output / case
            directory.mkdir()
            shutil.copy2(inputs["exe"], directory / "ProxyChainTests.exe")
            chain = case.startswith("chain-") or case == "edup"
            shutil.copy2(inputs["dll"], directory / ("d3d11_edhm.dll" if chain else "d3d11.dll"))
            if chain:
                outer = args.edup.resolve(strict=True) if case == "edup" else inputs["fixture"]
                shutil.copy2(outer, directory / "d3d11.dll")
            if case == "edup":
                settings = (
                    "[advanced]\nreal_dll=d3d11_edhm.dll\nd3d11_fixes=1\n"
                    "[fix]\ntemporal_aa=off\n[d3d11]\nfocus_on_launch=0\n[hotkey]\nmenu=F8\n"
                )
                for name in ("edvr.ini", "edvr-flat.ini"):
                    (directory / name).write_text(settings, encoding="utf-8")
            proxy = ""
            if case == "downstream":
                shutil.copy2(inputs["fixture"], directory / "downstream.dll")
                proxy = "downstream.dll"
            elif case == "missing-proxy":
                proxy = "missing.dll"
            elif case == "self-proxy":
                proxy = "d3d11.dll"
            elif case == "invalid-proxy":
                # A real Windows library without the two required D3D11 exports.
                system = (args.system_root or Path(os.environ["SystemRoot"])) / "system32" / "version.dll"
                shutil.copy2(system, directory / "invalid.dll")
                proxy = "invalid.dll"
            (directory / "d3dx.ini").write_text(
                "[System]\nload_library_redirect=0\nallow_platform_update=1\n"
                f"proxy_d3d11={proxy}\n"
                "[Hunting]\nhunting=0\n[Rendering]\nstereo_params=-1\nini_params=120\n"
                "[Logging]\nenabled=1\ncalls=0\ndebug=0\n",
                encoding="utf-8",
            )
            command = [str(directory / "ProxyChainTests.exe"), case]
            if args.runner:
                command.insert(0, args.runner)
            completed = subprocess.run(command, cwd=directory, capture_output=True, text=True, timeout=60, check=False)
            (directory / "stdout.txt").write_text(completed.stdout, encoding="utf-8")
            (directory / "stderr.txt").write_text(completed.stderr, encoding="utf-8")
            record = {"case": case, "exit_code": completed.returncode}
            records.append(record)
            if args.expect_bypass:
                record["passed"] = completed.returncode == 1 and (
                    "proxy device policy was bypassed" in completed.stderr
                    or "proxy swap-chain policy was bypassed" in completed.stderr
                )
            else:
                record["passed"] = completed.returncode == 0
                if case == "edup":
                    logs = "\n".join(path.read_text(encoding="utf-8", errors="replace")
                                     for path in (directory / "edvr_logs").glob("*gfx*.log"))
                    # These are EDUP's published startup/frame-boundary diagnostics,
                    # not calls into its private state or a replacement implementation.
                    record["edup_device_attached"] = "D3D11 device " in logs
                    record["edup_present_active"] = "Present is running on thread " in logs
                    record["passed"] = (record["passed"] and record["edup_device_attached"]
                                        and record["edup_present_active"])
            print(f"{'PASS' if record['passed'] else 'FAIL'} {case}: exit {completed.returncode}", flush=True)
            if not record["passed"]:
                raise RuntimeError(f"{case}: {completed.stdout}\n{completed.stderr}")
    finally:
        evidence = {
            "inputs": {
                name: {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                for name, path in inputs.items()
            },
            "expected_bypass": args.expect_bypass,
            "edup_sha256": hashlib.sha256(args.edup.read_bytes()).hexdigest() if args.edup else None,
            "cases": records,
        }
        (output / "results.json").write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
