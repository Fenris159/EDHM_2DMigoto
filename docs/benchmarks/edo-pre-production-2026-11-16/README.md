# Preliminary publication and shader-preparation evidence

These are off-game preparation receipts for `pre-production-edo-readiness` on
16 November 2026. They support the [main plan](../../edo-next-readiness-2026-11-16.md)
and [implementation status](../../pre-production-edo-readiness.md). They do not
compare shipped DLLs, original 3Dmigoto, FPS or Present-loop performance.

## Successful upload kernels

One executable compares the existing checked Map/copy/Unmap body with the new
checked full-image D3D adapter. It uses the same compiler, device, texture and
CPU image. Each workload has 21 samples of four 50,000-upload batches, alternating
A/B/B/A and B/A/A/B. Correctness readback is outside the measured section.
One row is 16 bytes; 341 rows are 5,456 bytes, matching the inspected pack's
largest lexical row requirement. This probe excludes the command interpreter,
transitions, pending-request policy and full DLL.

| Rows | Run 1: paired CPU-time change | Run 2: paired CPU-time change |
| --- | ---: | ---: |
| 1 | +1.59% | −3.72% |
| 341 | +0.39% | −3.16% |

Positive means the new checked kernel took more CPU time. Each number is the
median per-sample ratio of summed B durations to summed A durations, minus one;
it is not the ratio of separately calculated medians. The direction changes
between runs. **No speed improvement is established.** The production change's
demonstrated purpose is safer failure handling and retained retry requests.
Native shared-harness command-entry measurements remain a separate gate.

Raw samples: [run 1](upload-run-1.csv), [run 2](upload-run-2.csv).
[Provenance](upload-provenance.json) records source/executable hashes, compiler
arguments, host and environment. The runs used MinGW GCC 13, Wine 10 and Xvfb.
Wine reported an NVIDIA GTX 470 adapter; the physical host has AMD Strix Halo,
and the headless GL diagnostic reported unaccelerated Mesa llvmpipe. The reported
adapter name therefore does **not** establish NVIDIA hardware coverage. These
results establish neither Windows WARP nor Proton or hardware-vendor performance.

## Correctness and passive inventory

The [draw receipt](draw-provenance.json) covers two real offscreen draws through
the production upload adapter and publication policy. With no explicit flush
or readback between them, the final float pixels must be `(9,10,11,12)` and
`(40,30,20,10)`. The clean helper passes; deliberately dropping its first
publication fails that exact final-image assertion. This is an adapter check,
not a product DLL hook or future-host rendering result.

All [eleven isolated mutations](mutation-provenance.json) were detected, including
retained-request, complete-copy, input-preservation and provenance defects.
Restored policy, inventory and concrete D3D checks pass. The product runtime's
four new wiring mutations are still awaiting native Windows execution.

The [passive inventory receipt](inventory-provenance.json) records one pack's
counts, source-tool hash and 87 rechecked file digests. There are nine candidate
INIs, 77 assembly replacements and one separately recorded unreached dev INI.
Its contents do not contribute to section counts. The nine adjacent texture
mentions in the root are inspection candidates with unverified reuse legality;
they do not measure query cost. Full personal configuration stays local.
Inspection [classifies eight pairs as effectful command then query](query-candidates.json)
and one as assignment then query. Measure that single repeated-read example
first; the count does not justify a general cache or crossing command effects.

## Separate backend checks

The same approved adapter/draw behaviors were also checked through copied DXVK
DLLs from the installed Proton Experimental. The process still used Wine 10;
this is not a full Proton runtime or product DLL test. DXVK selected the AMD
Strix Halo adapter with RADV Mesa 26.0.8 and D3D feature level 11_0.

| Route | Result |
| --- | --- |
| Wine built-in, 64-bit GCC 13 / Clang 23 | Adapter, draw ordering and recreation pass |
| Wine built-in, 32-bit Clang 23 | Portable policy and draw-only check pass; full adapter check fails |
| Wine + DXVK, 32-bit Clang 23 | Full adapter and draw checks pass on AMD |
| Wine + DXVK, 64-bit GCC 13 | Full adapter and draw checks pass on AMD |
| Native Windows MSVC / WARP | Not run |

The 32-bit built-in failure is retained. The second raw staging read already
has a wrong first value before the CPU copy and next upload. It reproduces
without the Map interception and with the old checked Map/copy/Unmap kernel.
Disabling optimization or retaining staging references does not remove it; the
unchanged executable passes with DXVK. This isolates a backend-dependent result
in this experiment, with the exact root cause unresolved. Production code and
assertions were not changed to hide it. Native Win32 validation remains required.

[Backend provenance](backend-provenance.json) pins both architectures' DLLs and
executables, environment and outcomes. The additional 32-bit compiler came from
the [LLVM-MinGW 20261104 release](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261104);
its published archive digest was verified before extraction. This adds bounded
AMD/DXVK adapter evidence, with no NVIDIA/Intel, EDUP, game or future-SDK claim.

An additional [Proton runtime receipt](proton-provenance.json) runs the same
fixture inside Steam Linux Runtime sniper with the installed Proton 11.0-2e.
Both architectures pass through its native DXVK 2.7.1 build on AMD RADV. Both
also pass its built-in D3D route; a 32-bit loader trace confirms the built-in
modules. The Wine 10 failure therefore does not reproduce in that newer
runtime/container, although its exact cause remains unresolved.

These runs use an isolated compatibility prefix and direct `runinprefix`
invocations, requiring both zero exit and the exact fixture pass message.
An earlier normal launcher attempt returned zero without captured assertion
output and is excluded from positive evidence. This validates the adapter
fixture under those runtime routes, without installing or testing the product
DLL, launching Elite Dangerous, creating a swap chain or proving game compatibility.

## One copied assembly input

The existing upstream 3Dmigoto 1.4.9 command-line helper assembled one copied
4,379-byte `ps_5_0` HUD replacement into a 2,432-byte DXBC container. The helper
was paired with an architecture-matched x86 Microsoft `d3dcompiler_47.dll`, using
a native-only Wine override. Its Flugan disassembly/reassembly verification
passed. Microsoft disassembly also succeeded. The resulting container has five
input-signature entries, one output-signature entry and a SHEX code chunk.
The [assembly receipt](assembly-provenance.json) records input/tool/output hashes.
The installed source asset's digest remained unchanged.

Microsoft's default textual disassembly printed `1.96078438e-02` as `0.019608`.
Those literals encode different float32 values (`a1a0a03c` versus `f5a0a03c`,
little endian). Reassembly of that text changed four code words and failed the
strict comparison. A default human-readable disassembly is therefore unsuitable
as this input's byte-exact validation oracle. An earlier mismatched-bitness
compiler attempt is excluded from the positive result.

The stock helper overwrites a same-basename staged `.asm` during disassembly;
the [upstream output routine](https://github.com/bo3b/3Dmigoto/blob/8f329bd94fecc9bbcb9211ffd42a95dd7fe6b43e/HLSLDecompiler/cmd_Decompiler/cmd_Decompiler.cpp#L502)
opens its output for writing. A future preparer must keep immutable input copies
and separate assembly/validation directories, pin both executable architectures,
and reject implicit compiler fallback.

This is one reuse proof with an existing upstream helper. No new local preparer,
shader-object creation, rendered-output comparison or future-host check ran.
The filename is a matching claim; original game container/build identity remains
unverified. Shader assets and redistributable binaries are not included here.
Asset permissions, negative cases and native validation remain gates before
shipping a prepared pack or claiming rank 4 complete.

The subsequent [full-pack receipt](assembly-pack-provenance.json) extends the
bounded reuse experiment to all 77 inventoried replacements: 74 pixel shaders
and three vertex shaders. **All 77 assembled, passed the Flugan round trip and
were accepted by Microsoft disassembly.** Each asset used separate assembly and
validation directories; both staged inputs and installed source digests remained
unchanged. This establishes suitable inputs for a small preparer for this pack.
Original D3D11 register assignments remain; no `t120` relocation or future ABI
transform was attempted.
It does not remove the original-identity, negative-case, permission or rendering
gates above. Only names, sizes, hashes and outcomes are committed.
