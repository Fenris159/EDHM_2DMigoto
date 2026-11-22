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
    for folder in ("system32", "syswow64", "sysnative"):
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
    controls.add_argument("--shutdown-only", action="store_true")
    controls.add_argument("--d3d12-only", action="store_true")
    parser.add_argument("--expect-shutdown-failure", action="store_true")
    args = parser.parse_args()
    if args.expect_shutdown_failure and not args.shutdown_only:
        parser.error("--expect-shutdown-failure requires --shutdown-only")
    inputs = {name: getattr(args, name).resolve(strict=True) for name in ("dll", "fixture", "exe")}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    mixed_cases = ["chain-downstream-lazy", "chain-downstream-early", "chain-downstream-swap-early"]
    control = args.expect_bypass or args.expect_downstream_bypass
    cases = mixed_cases if args.expect_downstream_bypass else ["chain-lazy", "chain-early", "chain-swap-early"] + mixed_cases
    if not control:
        cases += [
            "standalone",
            "downstream",
            "missing-proxy",
            "self-proxy",
            "invalid-proxy",
            "feature-level-default",
            "feature-level-force",
            "chain-feature-level",
        ]
    if args.edup:
        cases += ["edup"]
    if args.shutdown_only:
        cases = ["standalone-shutdown", "chain-shutdown", "standalone-unload"]
        if args.edup:
            cases += ["edup-shutdown"]
    if args.d3d12_only:
        cases = ["d3d12-hwnd", "d3d12-legacy", "d3d12-composition", "d3d12-corewindow"]
    records = []
    target_machine = pe_machine(inputs["exe"])
    try:
        for case in cases:
            directory = output / case
            directory.mkdir()
            shutil.copy2(inputs["exe"], directory / "ProxyChainTests.exe")
            chain = case.startswith("chain-") or case.startswith("edup")
            shutil.copy2(inputs["dll"], directory / ("d3d11_edhm.dll" if chain else "d3d11.dll"))
            if chain:
                outer = args.edup.resolve(strict=True) if case.startswith("edup") else inputs["fixture"]
                shutil.copy2(outer, directory / "d3d11.dll")
            if case.startswith("edup"):
                settings = (
                    "[advanced]\nreal_dll=d3d11_edhm.dll\nd3d11_fixes=1\n"
                    "[fix]\ntemporal_aa=off\n[d3d11]\nfocus_on_launch=0\n[hotkey]\nmenu=F8\n"
                )
                for name in ("edvr.ini", "edvr-flat.ini"):
                    (directory / name).write_text(settings, encoding="utf-8")
            if "shutdown" in case:
                shutil.copy2(inputs["fixture"], directory / "ResourceRetainer.dll")
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
            device_settings = "[Device]\nwidth=640\nheight=480\n" if case.startswith("d3d12-") else ""
            create_device_setting = "allow_create_device=2\n" if case == "feature-level-force" else ""
            # Omit allow_platform_update in feature-level cases to verify its modern default,
            # and keep load_library_redirect=2 in chain-feature-level to verify renamed-DLL auto-disable.
            redirect_setting = "load_library_redirect=2\n" if case == "chain-feature-level" else "load_library_redirect=0\n"
            platform_setting = "" if "feature-level" in case else "allow_platform_update=1\n"
            (directory / "d3dx.ini").write_text(
                f"[System]\n{redirect_setting}{platform_setting}"
                f"{create_device_setting}"
                f"proxy_d3d11={proxy}\n"
                "[Hunting]\nhunting=0\n[Rendering]\nstereo_params=-1\nini_params=120\n"
                "[Logging]\nenabled=1\ncalls=0\ndebug=0\n" + device_settings,
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
            expected_creations = 1 if case.startswith("edup") or "shutdown" in case else 2 if "feature-level" in case else 4
            completed_creations = sum(line.startswith(f"PASS {case} creation ") for line in completed.stdout.splitlines())
            record = {"case": case, "exit_code": completed.returncode,
                      "completed_creations": completed_creations, "passed": False}
            records.append(record)
            if case.startswith("edup"):
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
                elif "feature-level" in case:
                    lines = completed.stdout.splitlines()
                    record["modern_interfaces_checked"] = f"PASS {case} modern interfaces" in lines
                    record["downlevel_policy_checked"] = f"PASS {case} downlevel policy" in lines
                    record["passed"] = (creations_ok and record["modern_interfaces_checked"]
                                        and record["downlevel_policy_checked"])
                else:
                    record["passed"] = creations_ok
            if case.startswith("d3d12-"):
                try:
                    log = (directory / "d3d11_log.txt").read_text(encoding="utf-8", errors="replace")
                except OSError as error:
                    record["log_read_error"] = f"{type(error).__name__}: {error}"
                    record["no_false_dx12_warning"] = False
                else:
                    record["no_false_dx12_warning"] = ("FATAL: Unsupported DirectX Version!" not in log
                                                       and "3DMigoto does not support DirectX 12" not in log)
                record["passed"] = record["passed"] and record["no_false_dx12_warning"]
            if "shutdown" in case:
                completion = directory / "shutdown-complete.txt"
                record["completion_bytes"] = completion.read_bytes().hex() if completion.is_file() else None
                record["retained_release_completed"] = completion.is_file() and completion.read_bytes() == b"released\n"
                record["passed"] = record["passed"] and record["retained_release_completed"]
                if args.expect_shutdown_failure:
                    # Creation and the opened receipt prove setup succeeded.
                    # Windows can suppress an exception during process teardown,
                    # so an exit code alone is insufficient for this control.
                    setup_ok = completed_creations == 1
                    if case.startswith("edup"):
                        setup_ok = (setup_ok and record["edup_chain_selected"]
                                    and record["edup_device_attached"] and record["edup_present_active"])
                    record["passed"] = setup_ok and completion.is_file() and completion.read_bytes() == b""
            print(f"{'PASS' if record['passed'] else 'FAIL'} {case}: exit {completed.returncode}", flush=True)
            if not record["passed"] and not args.d3d12_only:
                raise RuntimeError(f"{case}: {completed.stdout}\n{completed.stderr}")
        if args.d3d12_only and any(not record["passed"] for record in records):
            raise RuntimeError("D3D12 helper swap-chain checks failed; see per-case evidence")
    finally:
        evidence = {
            "inputs": {
                name: {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                for name, path in inputs.items()
            },
            "expected_bypass": args.expect_bypass,
            "shutdown_only": args.shutdown_only,
            "d3d12_only": args.d3d12_only,
            "expected_shutdown_failure": args.expect_shutdown_failure,
            "expected_downstream_bypass": args.expect_downstream_bypass,
            "target_machine": target_machine,
            "edup_sha256": hashlib.sha256(args.edup.read_bytes()).hexdigest() if args.edup else None,
            "cases": records,
        }
        (output / "results.json").write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
