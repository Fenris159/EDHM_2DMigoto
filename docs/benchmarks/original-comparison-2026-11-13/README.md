# Original 3Dmigoto comparison evidence — 2026-11-13

Read the [report](../../develop-vs-original-3dmigoto.md) for interpretation and
the [compatibility appendix](../original-comparison-compatibility.md) for primary
API/vendor sources. This archive preserves off-game measurements of develop
`5c74823ff0d21c662e2d659d04afc6624d245509` against original 3Dmigoto 1.4.9
`4ce5f2f72b2777223d2e809dcdafec514ac98295`. A means original, B develop and R
the system D3D11 control. S is historical 1.3.16 context, with no accepted timing.

## Contents and provenance

| Path | Contents |
| --- | --- |
| [provenance.json](provenance.json) | Source/asset identities, SHA-256 hashes, hosts, tools, exclusions and accepted run IDs |
| [summary.json](summary.json) | Every process median, group median, change interval and complete native trace method counts |
| [components/](components/) | Sixteen raw CSV files and binary/build/runner identities; no shipped-DLL timing in this experiment |
| [native-warp/](native-warp/) | Eighteen actual-DLL Windows WARP JSONL results, manifest, shared INIs and stderr |
| [amd-proton/](amd-proton/) | Eighteen actual-DLL AMD/RADV/Proton JSONL results, manifest, shared INIs and stderr |
| [native-trace/](native-trace/) | Five clean, untimed lower-layer API captures, gzip-compressed decoded calls, outputs and configurations |
| [checker-sensitivity/](checker-sensitivity/) | Six controlled caller faults, exact source patches, stdout/stderr and build/failure receipt |
| [stable-context/](stable-context/) | Excluded 1.3.16 preflight: exit 5, no output metrics; its manifest also records the preceding raw control |
| [SHA256SUMS](SHA256SUMS) | Checksums of every other file in this archive, including this guide |

Raw CSV/JSONL, manifests, configuration and stderr are copied byte-for-byte.
Decoded traces are losslessly gzip-compressed; binary `.trace` files are
unchanged. Local Git attributes prevent newline conversion of these receipts.
Sensitivity patches compare the unchanged caller with separately
compiled fault copies. Executables, redistributable DLLs, shader caches and
full CI logs are not committed. The archived hashes identify measured binaries;
the fixture source and build steps identify how to reproduce the workload.

Accepted GitHub runs:

- [Develop DLL build, 43441909497](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43441909497).
- [Original/develop source components, 43447201014](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43447201014).
- [Actual DLL Windows timing, 43449364115](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43449364115).
- [Clean native API traces, 43452193512](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43452193512).

Proton measurements and checker-sensitivity runs were executed locally on the
AMD host identified in provenance. Both native and Proton timing collections
have 18/18 passing processes: four A and four B per profile plus two passive R
controls. Each of the five native trace cases exits zero. Failed diagnostic
attempts are listed in provenance and are not pooled with accepted samples.

## Recalculate without running D3D11

The standard-library-only summarizer is pinned at
[`c66db49a`](https://github.com/Fenris159/EDHM_2DMigoto/blob/c66db49af0602286ee448557ca2123e5a35d120c/Benchmarks/summarize_original_comparison.py)
on `original-3dmigoto-performance`. From a checkout of that revision, supply
the path to this archive in your develop checkout:

```bash
python Benchmarks/summarize_original_comparison.py \
  /path/to/develop/docs/benchmarks/original-comparison-2026-11-13 \
  --output /tmp/original-comparison-summary.json
```

The result should equal `summary.json`. Checksums can be verified inside this
directory with `sha256sum --check SHA256SUMS`. To read an API dump, decompress
its `*.trace.txt.gz`; the binary capture can also be opened with apitrace 14.0.
Source objects and pointers in a dump identify that process only; compare
counts and public resource requirements across processes, not pointer values.

The summarizer drops sample 0 only for repeated timing metrics, retains each
startup/memory observation, forms per-process medians and then group medians,
and enumerates all n=4 independent bootstrap median-ratio combinations. It
does not treat within-process samples as independent processes. Wine's zero
PrivateUsage values become unavailable entries. Raw R medians are retained
for reference but do not replace A in percentage calculations.

## Repeat the executable experiments

Use an isolated checkout at the benchmark revision above. Production sources
are frozen at the report's SHAs; a new source/build/backend needs a new receipt.
The pinned workflows show exact steps and original archive checks:

- [Component workflow](https://github.com/Fenris159/EDHM_2DMigoto/blob/c66db49af0602286ee448557ca2123e5a35d120c/.github/workflows/original-comparison.yml).
- [Actual DLL workflow](https://github.com/Fenris159/EDHM_2DMigoto/blob/c66db49af0602286ee448557ca2123e5a35d120c/.github/workflows/offscreen-comparison.yml).
- [Trace workflow](https://github.com/Fenris159/EDHM_2DMigoto/blob/c66db49af0602286ee448557ca2123e5a35d120c/.github/workflows/trace-surface-comparison.yml).

For native Windows, build the caller from a VS 2022 developer PowerShell:

```powershell
msbuild Benchmarks/SurfaceProbe.vcxproj -target:Build `
  -property:Configuration=Release -property:Platform=x64 `
  -nodeReuse:false -verbosity:minimal
python Benchmarks/run_surface_probe.py `
  --probe .quality/surface-probe-native/SurfaceProbe.exe `
  --compiler C:/bench/original/x64/d3dcompiler_47.dll `
  --variant A=C:/bench/original/x64/d3d11.dll `
  --variant B=C:/bench/develop/d3d11.dll --variant R=system `
  --backend warp --samples 7 --iterations 2000 --output .quality/new-native-results
```

Use original's pinned archive and the exact develop CI artifact if still
available. CI artifacts have finite retention; rebuilding the pinned source
later may change its DLL hash. Record the new binary and toolchain rather than
claiming byte identity. The runner copies each caller beside its selected DLL
to satisfy the original target guard and includes original's shipped NVAPI
sidecar. Common compiler/configuration hashes must match before comparing.

For the recorded Linux caller, build with MinGW:

```bash
x86_64-w64-mingw32-g++ -std=c++17 -O2 -Wall -Wextra -Werror -static -municode \
  Benchmarks/SurfaceProbe.cpp -o /tmp/SurfaceProbe.exe -lpsapi -ldxguid
python Benchmarks/run_surface_probe.py \
  --probe /tmp/SurfaceProbe.exe --compiler /path/to/original/x64/d3dcompiler_47.dll \
  --variant A=/path/to/original/x64/d3d11.dll \
  --variant B=/path/to/develop/d3d11.dll --variant R=system \
  --backend hardware --proton '/path/to/Proton 11.0/proton' \
  --compatdata /path/to/dedicated-offscreen-prefix --steam-root /path/to/Steam \
  --samples 7 --iterations 2000 --output /path/to/new-proton-results
```

Prepare a dedicated initialized test prefix, never an installed game's prefix.
The runner uses `runinprefix`, disables Xalia and fixes the AMD filter; adapt
the filter deliberately for a new vendor and record it. Both caller builds
use C++17 and warnings-as-errors. Do not compare absolute native/Proton times
as if the different caller toolchains, CPUs and backends were held constant.
The primary timing caller source was `597195f9`; the final harness adds an
environment-controlled diagnostic unload used only during trace runs.

The trace workflow sets `proxy_d3d11=dxgitrace.dll`, places the unrenamed tracer
beside each caller, and enables `EDHM_PROBE_UNLOAD_RUNTIME=1` after caller COM
objects are released. R loads that tracer directly. This needs no global
injection. Tracing can shadow mapped memory and perform its own serialization,
so its times and private commit are excluded. See the
[pinned tracer implementation](https://github.com/apitrace/apitrace/blob/14.0/wrappers/dxgitrace.py#L146)
and [DXVK's tracing guide](https://github.com/doitsujin/dxvk/wiki/Using-Apitrace).

The checker-sensitivity patches apply individually to
`Benchmarks/SurfaceProbe.cpp` at `9549ff50`; compile each in a disposable
directory and require exit 1 with the receipt's named assertion. First five
faults use the raw AMD/DXVK path; `hud-target` uses the actual develop DLL/HUD
profile. Restore the unchanged caller before accepting any timing collection.
No runtime DLL is patched by those checks.
