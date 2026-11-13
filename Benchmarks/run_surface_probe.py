"""Run isolated offscreen DLL fixtures; never inspect or launch a game."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def guest(path):
    path = path.resolve()
    return str(path) if os.name == "nt" else "Z:" + str(path).replace("/", "\\")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--variant", action="append", default=[], help="A=original.dll, B=develop.dll, optional S=stable.dll, R=system")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--backend", choices=("warp", "hardware"), required=True)
    parser.add_argument("--proton", type=Path)
    parser.add_argument("--compatdata", type=Path)
    parser.add_argument("--steam-root", type=Path)
    parser.add_argument("--samples", type=int, default=7)
    parser.add_argument("--iterations", type=int, default=2000)
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--trace-proxy", type=Path)
    parser.add_argument("--force-module-path", type=Path)
    args = parser.parse_args()
    if not 1 <= args.samples <= 100 or not 1 <= args.iterations <= 100000:
        parser.error("invalid sample or iteration count")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if (output / "manifest.json").exists():
        parser.error("choose a new output directory; previous receipts are immutable")
    variants = dict(item.split("=", 1) for item in args.variant)
    if not {"A", "B"}.issubset(variants) or not set(variants).issubset({"A", "B", "R", "S"}):
        parser.error("provide A and B, with optional R/S")
    env = os.environ.copy()
    command_prefix = []
    if args.proton:
        if not args.compatdata or not args.steam_root:
            parser.error("Proton requires isolated --compatdata and --steam-root")
        args.compatdata.resolve().mkdir(parents=True, exist_ok=True)
        env.update(STEAM_COMPAT_DATA_PATH=str(args.compatdata.resolve()),
                   STEAM_COMPAT_CLIENT_INSTALL_PATH=str(args.steam_root.resolve()),
                   SteamAppId="0", SteamGameId="0", STEAM_COMPAT_APP_ID="0",
                   WINEDEBUG="-all", PROTON_USE_XALIA="0", DXVK_LOG_LEVEL="none",
                   DXVK_FILTER_DEVICE_NAME="AMD Radeon",
                   WINEDLLOVERRIDES="d3d11,dxgi=n;d3dcompiler_47=n,b",
                   XDG_CACHE_HOME=str(output / "cache"))
        command_prefix = [str(args.proton.resolve()), "runinprefix"]
    if args.force_module_path:
        env["APITRACE_FORCE_MODULE_PATH"] = guest(args.force_module_path)
    if args.trace_proxy:
        env["EDHM_PROBE_UNLOAD_RUNTIME"] = "1"
    manifest = {
        "probe_sha256": digest(args.probe), "compiler_sha256": digest(args.compiler),
        "backend": args.backend, "proton": str(args.proton.name) if args.proton else None,
        "os": platform.platform(), "processor": platform.processor(),
        "samples": args.samples, "iterations": args.iterations,
        "profiling": "off", "debug_layer": False, "game": False, "present": False,
        "trace_proxy_sha256": digest(args.trace_proxy) if args.trace_proxy else None,
        "variants": {}, "runs": [], "status": "running",
    }
    fixtures = {}
    for variant, source in variants.items():
        folder = output / "fixtures" / variant
        folder.mkdir(parents=True)
        shutil.copy2(args.probe, folder / "SurfaceProbe.exe")
        if args.trace_proxy:
            shutil.copy2(args.trace_proxy, folder / "dxgitrace.dll")
        if source == "system":
            runtime = guest(folder / "dxgitrace.dll") if args.trace_proxy else "system"
            manifest["variants"][variant] = {"runtime": "system"}
        else:
            source = Path(source).resolve()
            shutil.copy2(source, folder / "d3d11.dll")
            nvapi = source.parent / "nvapi64.dll"
            if nvapi.exists():
                shutil.copy2(nvapi, folder / nvapi.name)
            runtime = guest(folder / "d3d11.dll")
            manifest["variants"][variant] = {"dll_sha256": digest(source), "dll_bytes": source.stat().st_size}
        fixtures[variant] = (folder, runtime)
    manifest_path = output / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    failures = []
    try:
        for profile in ("passive", "hud"):
            config = subprocess.run(command_prefix + [str(args.probe.resolve()), "--config", guest(args.compiler), profile],
                                    env=env, capture_output=True, timeout=180)
            (output / f"{profile}-config.stderr").write_bytes(config.stderr)
            if config.returncode or b"[Logging]" not in config.stdout:
                raise RuntimeError(f"configuration generation failed: {profile}")
            configuration = config.stdout
            if args.trace_proxy:
                configuration = configuration.replace(b"load_library_redirect=0", b"load_library_redirect=0\nproxy_d3d11=dxgitrace.dll")
            (output / f"{profile}.ini").write_bytes(configuration)
            for folder, _ in fixtures.values():
                (folder / "d3dx.ini").write_bytes(configuration)
            if args.smoke:
                order = list(variants)
            else:
                order = (["R"] if "R" in variants and profile == "passive" else [])
                order += (["S"] if "S" in variants else [])
                order += ["A", "B", "B", "A", "B", "A", "A", "B"]
                order += (["S"] if "S" in variants else [])
                order += (["R"] if "R" in variants and profile == "passive" else [])
            for trial, variant in enumerate(order):
                if profile == "hud" and variant == "R":
                    continue
                folder, runtime = fixtures[variant]
                run_env = env.copy()
                if args.trace_proxy:
                    trace = output / f"{profile}-{trial}-{variant}.trace"
                    run_env["TRACE_FILE"] = guest(trace)
                command = command_prefix + [str(folder / "SurfaceProbe.exe"), runtime, guest(args.compiler), args.backend,
                                            profile, str(args.samples), str(args.iterations)]
                result = subprocess.run(command, cwd=folder, env=run_env, capture_output=True, timeout=180)
                stem = f"{profile}-{trial}-{variant}"
                (output / f"{stem}.jsonl").write_bytes(result.stdout)
                (output / f"{stem}.stderr").write_bytes(result.stderr)
                records = [json.loads(line) for line in result.stdout.decode(errors="replace").splitlines() if line.startswith("{")]
                passes = {row["metric"] for row in records if row["unit"] == "pass" and row["value"] == 1}
                passed = not result.returncode and passes == {"targeted_and_untargeted_state", "color_depth_volume_compute"}
                run = {"profile": profile, "trial": trial, "variant": variant, "exit": result.returncode,
                       "pass": passed, "record_count": len(records)}
                manifest["runs"].append(run)
                manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
                print(json.dumps(run), flush=True)
                if not passed:
                    failures.append(run)
                    print(result.stderr.decode(errors="replace")[-1500:], flush=True)
                    if not args.smoke:
                        raise RuntimeError("failed output validation; do not accept timing data")
        manifest["status"] = "failed" if failures else "passed"
    finally:
        if manifest["status"] == "running":
            manifest["status"] = "interrupted"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
