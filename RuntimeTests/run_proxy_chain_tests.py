"""Exercise the built product DLL across a third-party D3D11 proxy boundary."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess


def pe_machine(path):
    data = path.read_bytes()
    if len(data) < 64 or data[:2] != b"MZ":
        raise ValueError(f"Not a PE file: {path}")
    header = struct.unpack_from("<I", data, 60)[0]
    if header + 6 > len(data) or data[header:header + 4] != b"PE\0\0":
        raise ValueError(f"Invalid PE header: {path}")
    return struct.unpack_from("<H", data, header + 4)[0]


def matching_version_library(system_root, target_machine):
    for folder in ("system32", "SysWOW64", "Sysnative"):
        candidate = system_root / folder / "version.dll"
        if candidate.is_file() and pe_machine(candidate) == target_machine:
            return candidate
    raise RuntimeError(f"No loadable version.dll for PE machine 0x{target_machine:04x}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dll", required=True, type=Path)
    parser.add_argument("--fixture", required=True, type=Path)
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--runner", default="")
    parser.add_argument("--system-root", type=Path)
    parser.add_argument("--edup", type=Path)
    controls = parser.add_mutually_exclusive_group()
    controls.add_argument("--expect-bypass", action="store_true")
    controls.add_argument("--expect-downstream-bypass", action="store_true")
    args = parser.parse_args()
    inputs = {name: getattr(args, name).resolve(strict=True) for name in ("dll", "fixture", "exe")}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    mixed_cases = ["chain-downstream-lazy", "chain-downstream-early", "chain-downstream-swap-early"]
    control = args.expect_bypass or args.expect_downstream_bypass
    cases = mixed_cases if args.expect_downstream_bypass else ["chain-lazy", "chain-early", "chain-swap-early"] + mixed_cases
    if not control:
        cases += ["standalone", "downstream", "missing-proxy", "self-proxy", "invalid-proxy"]
    if args.edup:
        cases += ["edup"]
    records = []
    target_machine = pe_machine(inputs["exe"])
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
            if "downstream" in case:
                shutil.copy2(inputs["fixture"], directory / "downstream.dll")
                proxy = "downstream.dll"
            elif case == "missing-proxy":
                proxy = "missing.dll"
            elif case == "self-proxy":
                proxy = "d3d11.dll"
            elif case == "invalid-proxy":
                # A real Windows library without the two required D3D11 exports.
                system = matching_version_library(args.system_root or Path(os.environ["SystemRoot"]), target_machine)
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
            try:
                completed = subprocess.run(command, cwd=directory, capture_output=True, text=True, timeout=60, check=False)
            except subprocess.TimeoutExpired as error:
                for name, captured in (("stdout", error.stdout), ("stderr", error.stderr)):
                    text = captured.decode("utf-8", errors="replace") if isinstance(captured, bytes) else captured or ""
                    (directory / f"{name}.txt").write_text(text, encoding="utf-8")
                records.append({"case": case, "timeout": True, "passed": False})
                raise
            (directory / "stdout.txt").write_text(completed.stdout, encoding="utf-8")
            (directory / "stderr.txt").write_text(completed.stderr, encoding="utf-8")
            expected_creations = 1 if case == "edup" else 4
            completed_creations = sum(line.startswith(f"PASS {case} creation ") for line in completed.stdout.splitlines())
            record = {"case": case, "exit_code": completed.returncode,
                      "completed_creations": completed_creations, "passed": False}
            records.append(record)
            if case == "edup":
                logs = "\n".join(path.read_text(encoding="utf-8", errors="replace")
                                 for path in (directory / "edvr_logs").glob("*gfx*.log"))
                # EDUP's published startup/frame-boundary diagnostics expose whether
                # the actual outer DLL is active, beyond merely loading successfully.
                record["edup_chain_selected"] = "chaining through " in logs and "d3d11_edhm.dll" in logs
                record["edup_device_attached"] = "D3D11 device " in logs
                record["edup_present_active"] = "Present is running on thread " in logs
                wanted = not args.expect_bypass
                record["passed"] = (completed.returncode == 0 and completed_creations == expected_creations
                                    and record["edup_chain_selected"]
                                    and record["edup_device_attached"] == wanted
                                    and record["edup_present_active"] == wanted)
            elif args.expect_downstream_bypass:
                # The outer BGRA policy must survive while only the child
                # single-threaded policy disappears. An outer bypass is a
                # different regression and must not satisfy this control.
                record["outer_only_device_policy"] = (
                    "Device policy: wanted 0x21, received 0x20" in completed.stderr.splitlines()
                )
                record["passed"] = (completed.returncode == 1 and record["outer_only_device_policy"]
                                    and "proxy device policy was bypassed" in completed.stderr)
            elif args.expect_bypass:
                record["passed"] = completed.returncode == 1 and (
                    "proxy device policy was bypassed" in completed.stderr
                    or "proxy swap-chain policy was bypassed" in completed.stderr
                )
            else:
                creations_ok = completed.returncode == 0 and completed_creations == expected_creations
                if case == "invalid-proxy":
                    log = (directory / "d3d11_log.txt").read_text(encoding="utf-8", errors="replace")
                    record["invalid_proxy_machine"] = pe_machine(directory / "invalid.dll")
                    record["missing_exports_rejected"] = "Rejected chained d3d11: required exports are missing" in log
                    record["passed"] = (creations_ok and record["invalid_proxy_machine"] == target_machine
                                        and record["missing_exports_rejected"])
                else:
                    record["passed"] = creations_ok
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
            "expected_downstream_bypass": args.expect_downstream_bypass,
            "target_machine": target_machine,
            "edup_sha256": hashlib.sha256(args.edup.read_bytes()).hexdigest() if args.edup else None,
            "cases": records,
        }
        (output / "results.json").write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
