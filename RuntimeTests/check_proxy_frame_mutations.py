"""Deliberately break approved frame behaviors; restore and rebuild production."""
import hashlib
import json
from pathlib import Path
import sys

import check_mutations as engine


def main():
    root = engine.ROOT
    engine.OUTPUT = root / ".quality" / "proxy-frame-mutations"
    engine.OUTPUT.mkdir(parents=True, exist_ok=False)
    engine.EXE = root / "x64" / "Release" / "d3d11.dll"
    engine.BUILD = [argument.replace("DirectX11/RuntimeTests.vcxproj", "DirectX11/DirectX11.vcxproj")
                    for argument in engine.BUILD]
    original_run = engine.run

    def run(command, log_name):
        if command[0] == str(engine.EXE):
            command = [sys.executable, "RuntimeTests/run_proxy_frame_tests.py", "--dll", str(engine.EXE),
                       "--exe", str(root / ".quality/proxy-frame/x64/ProxyFrameTests.exe"),
                       "--fixture", str(root / ".quality/proxy-frame/x64/FrameProxyFixture.dll"),
                       "--case", command[1], "--output", str(engine.OUTPUT / Path(log_name).stem)]
        return original_run(command, log_name)

    engine.run = run
    path = root / "DirectX11" / "HackerDXGI.cpp"
    original = path.read_bytes()
    text = original.decode("utf-8").replace("\r\n", "\n")
    mutations = [
        ("flow--pre", "\tRunFrameActions();\n\tif (profiling)\n\t\tProfiling::end(&state,",
         "\t// Deliberate break: omit pre commands.\n\tif (profiling)\n\t\tProfiling::end(&state,"),
        ("flow--post", "\tRunCommandList(mHackerDevice, mHackerContext, &G->post_present_command_list, nullptr, true);\n"
         "\tif (profiling)\n\t\tProfiling::end(&state,",
         "\t// Deliberate break: omit post commands.\n\tif (profiling)\n\t\tProfiling::end(&state,"),
        ("flow--foreign", "\tif (thread && thread != GetCurrentThreadId())\n\t\treturn false;",
         "\t// Deliberate break: accept another thread."),
        ("flow--secondary", "\tif (owner && owner != this)\n\t\treturn false;",
         "\t// Deliberate break: accept another chain."),
        ("controls--input", "newEvent = DispatchInputEvents(mHackerDevice);", "newEvent = false;"),
        ("controls--preset", "CurrentTransition.UpdatePresets(mHackerDevice);",
         "// Deliberate break: leave presets stale."),
        ("controls--transition", "CurrentTransition.UpdateTransitions(mHackerDevice);",
         "// Deliberate break: leave transitions stale."),
        ("controls--reload", "// instead and we handle it now.\n\tif (G->gReloadConfigPending)\n\t\tReloadConfig(mHackerDevice);",
         "// instead and we handle it now.\n\tif (G->gReloadConfigPending)\n\t\tG->gReloadConfigPending = false;"),
    ]
    for api, case, following in (("Present", "flow", "GetBuffer"), ("Present1", "flow1", "IsTemporaryMonoSupported")):
        start = text.index(f"STDMETHODIMP HackerSwapChain::{api}(")
        end = text.index(f"HackerSwapChain::{following}(", start)
        function = text[start:end]
        mutations.append((f"{case}--duplicate", function,
                          function.replace("if (proxy_frame_chain || mProxyFramesActive.load())",
                                           "if (!proxy_frame_chain && !mProxyFramesActive.load())", 1)))
    restoration_cases = [(name, path, old, new) for name, old, new in mutations]
    evidence = []
    try:
        for case in ("flow", "controls", "flow1", "controls1"):
            code, _ = run([str(engine.EXE), case], f"{case}-baseline.log")
            if code:
                raise RuntimeError(f"Baseline must pass: {case}")
        for name, old, new in mutations:
            if text.count(old) != 1:
                raise RuntimeError(f"Mutation must match exactly one location: {name}")
            mutated = text.replace(old, new, 1).encode("utf-8")
            path.write_bytes(mutated)
            try:
                code, _ = run(engine.BUILD, f"{name}-build.log")
                if code:
                    raise RuntimeError(f"Mutation must compile: {name}")
                code, output = run([str(engine.EXE), engine.native_case(name)], f"{name}-mutant.log")
                receipt = json.loads((engine.OUTPUT / f"{name}-mutant" / "results.json").read_text())
                if code == 0 or receipt.get("exit_code") != 1 or "FAIL:" not in output:
                    raise RuntimeError(f"Behavioral assertion must detect mutation: {name}")
                evidence.append({"mutation": name, "detected": True,
                                 "source_sha256": hashlib.sha256(mutated).hexdigest()})
                print(f"Detected deliberate behavior break: {name}", flush=True)
            finally:
                path.write_bytes(original)
    finally:
        # Reuse the tested restoration path, including process-tree timeout
        # handling, original-error preservation and executable quarantine.
        engine.restore_production({path: original}, restoration_cases, evidence, sys.exc_info()[1])
    print("All proxy frame mutations detected; clean production rebuilt and passes.", flush=True)


if __name__ == "__main__":
    main()
