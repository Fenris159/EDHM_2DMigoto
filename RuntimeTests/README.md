# Native runtime checks

This executable compiles the engine's production source list with a console entry
point. The normal DLL build keeps its existing entry points. Tests exercise public
parser, command, persistence and D3D boundaries. D3D cases use WARP and request the
debug layer; a missing SDK layer is printed as an explicit limitation.

From a Visual Studio 2022 developer PowerShell at the repository root:

```powershell
msbuild DirectX11/RuntimeTests.vcxproj -target:Build `
  -property:Configuration=Release -property:Platform=x64 `
  "-property:SolutionDir=$pwd/" -property:EdhmStrictBuild=true `
  -nodeReuse:false -verbosity:minimal -restore:false
& .quality/runtime-tests/x64/Release/RuntimeTests.exe
```

Use `Platform=Win32` and the corresponding output directory for 32-bit checks.
Pass a case name, such as `store-values` or `settings-lifecycle`, to run it in its
own process. CI runs every case separately, including both Windows architectures.

The suite covers expression syntax, settings retry/atomic replacement/retention,
pool reset/expiry/identifiers/static metadata, resource reference chains and
cycles, typed/structured readback and owned views, buffer region counts and
bounds, CB ranges, and INI/optimizer ordering and reload semantics. A real device
boundary records submitted buffer descriptors and can fail staging allocation;
this catches errors that WARP normalizes in a completed buffer description.

The 42 cases also cover monitor-mode/window/reload transitions and recursive or
shared command counts; summary-only matching/view diagnostics; hash-first and
ordered dual matching; documented expression operators; all four view kinds,
resource/descriptor/raw/override fallbacks, changing `this` targets, warm-cache
metadata changes, custom/pipeline ownership, and failed-view binding preservation.
The PR #43 cases additionally protect smaller constant-buffer region copies,
clearing saved SRV-backed resources through writable views, runtime-learned pool
bind flags, scalar/local variable precedence over pool names, stable `this`
view-cache reuse with safe kind changes, and zero-frame reload measurements.
View failures are injected at the D3D device boundary. Missing custom-resource metadata returns zero bind/misc flags without allocation.
Skipped empty-source references preview nested indexes without allocating or
refreshing pool slots. Normal reads resume their expiration behavior afterward;
expired previews expose defaults without resetting stored data. Initially empty
and first-time configured sources retain learned destination flags. Detached
previews use the ordinary lookup rules for lazy resource indexes, sequential FIFO
allocation and same-frame expiration/reset. Resource operands cannot substantiate
live templates; accepted nested and self-indexed reads preserve one GPU identity.
CPU timing is never a regression threshold.

## Regression sensitivity

The manual **Runtime regression checks** workflow accepts `mutation_checks=true`.
Its disposable checkout builds each deliberate behavior break, requires the
focused case to fail, restores the exact source bytes, then rebuilds and reruns
the restored cases. Local use requires a clean isolated worktree because the
runner temporarily rewrites the production command-list, configuration,
resource-matching and profiling sources.

```powershell
python RuntimeTests/check_mutations.py
```

Evidence is written under `.quality/runtime-mutations/`. The runner restores
source bytes and rebuilds/checks production even when a mutation fails; a failed
restoration removes the executable and preserves the original mutation error.
CI uploads native executables only after a successful job. Failed jobs upload
logs separately, without an executable. A compile failure does not count as
successful detection.

Run the three hermetic compiler/process-boundary cleanup cases with:

```bash
python -m unittest discover -s RuntimeTests -p test_mutation_cleanup.py -v
```

## Configuration measurements

Copy the native executable to a separate snapshot directory containing `d3dx.ini`
and its resolved includes. Disable diagnostic logging and hunting consistently
for both builds. Run from the same snapshot with the same Wine prefix:

```bash
WINEDEBUG=-all WINEPREFIX="$PWD/.quality/benchmark-prefix" \
  wine .quality/config-snapshot/RuntimeTests.exe benchmark-load 21
WINEDEBUG=-all WINEPREFIX="$PWD/.quality/benchmark-prefix" \
  wine .quality/config-snapshot/RuntimeTests.exe benchmark-startup 21
```

`benchmark-load` times `LoadConfigFile` and runs the normal optimizer outside
that timer to release optimizer-owned lists between cycles. `benchmark-startup`
reports load and command-list optimizer time separately. These modes measure
CPU configuration work with the real optimizer and no device. Use an isolated snapshot, alternate independent variant processes, and
record source/binary hashes. Separate first-process observations from warm loads.
See [the implementation report](../docs/next-release-implementation.md).

## Interpreter and resource-view measurements

The executable also supports `benchmark-lists 11` and `benchmark-views 11`.
These use fixed synthetic inputs and do not load the game configuration.
`benchmark-lists` runs real assignment commands across 64 command lists and
reports profiling-off, summary, top-list and top-command modes separately.
`benchmark-views` runs real WARP resource references with the debug layer off,
reporting both a stable resource and a round-robin set of 32 resources. It records
Create*View API calls and the final bound-resource result alongside CPU timing.
These are mechanism measurements, not Elite frames or GPU-time measurements.

For a native same-host A/B comparison, dispatch **Runtime regression checks**
with `mechanism_benchmarks=true` and `benchmark_baseline_run` set to a successful
trusted push/manual run containing an x64 executable with these benchmark modes.
The workflow alternates eight fresh processes in `ABBABAAB` order on one Windows
runner, with 11 samples per workload/mode. Exclude sample zero from warm results;
report process medians as well as pooled observations. The artifact includes raw
CSV and binary hashes. A later game comparison must keep diagnostics off for the
frame-time outcome and use separate diagnostic captures to locate wrapper costs.
