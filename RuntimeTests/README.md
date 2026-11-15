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

The 49 cases also cover monitor-mode/window/reload transitions and recursive or
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
Hot resource reads retain their identity across repeated adoption. Spatial
previews preserve shared CPU data and its range, and STATIC targets keep their
parsed resource when the owning pool later becomes a proxy.
STATIC and dynamic aliases of one root slot share a provisional resource.
The cache-range case uses distinct FIFO keys so dropping the preview offset
changes required binding flags. Expiring aliases check actual SRV/UAV capability
and data in both operand orders, without assuming an evaluation order.
Spatial and FIFO copies keep their resolved destination key after an intervening
same-pool source lookup, without evaluating the destination index again.
CPU timing is never a regression threshold.

## Product DLL proxy chaining

`ProxyChainTests.vcxproj` builds a small caller and, with
`EdhmChainFixture=true`, a stand-in for a third-party D3D11 proxy. The caller
loads the actual product DLL beside its executable. The outer fixture changes
public device flags and swap-chain buffer counts; those observable results must
survive early initialization and repeated calls to both creation exports.
Separate processes cover standalone loading, an explicit downstream proxy,
an outer proxy with a downstream proxy together, and missing, self-referencing
or invalid proxy fallback. The invalid library matches the caller's PE
architecture, and the check requires its missing-export rejection diagnostic.
The checks make WARP
devices and hidden swap chains; they do not benchmark frame presentation.

```powershell
msbuild DirectX11/DirectX11.vcxproj -target:Build `
  -property:Configuration=Release -property:Platform=x64 `
  "-property:SolutionDir=$pwd/" -property:EdhmStrictBuild=true `
  -nodeReuse:false -verbosity:minimal -restore:false
msbuild RuntimeTests/ProxyChainTests.vcxproj -property:Configuration=Release -property:Platform=x64
msbuild RuntimeTests/ProxyChainTests.vcxproj -property:Configuration=Release -property:Platform=x64 -property:EdhmChainFixture=true
python RuntimeTests/run_proxy_chain_tests.py --dll x64/Release/d3d11.dll `
  --fixture .quality/proxy-chain/x64/ProxyChainFixture.dll `
  --exe .quality/proxy-chain/x64/ProxyChainTests.exe `
  --output .quality/proxy-chain/manual-results
```

Use a new output directory for each run. The runtime workflow runs both Windows
architectures. Its x64 job also checks the pinned official EDUP 0.22.1 DLL:
a capability-only probe precedes real device creation, and EDUP must report
device attachment and an active Present callback. This checks graphics startup;
it does not establish Elite-specific AA output or an in-game F8 interaction.

The optional `proxy_chain_baseline_run` workflow input supplies a successful,
trusted x64 product build for the sensitivity control. Each chain case must fail
on the specific missing device/swap-chain policy; a crash or build failure does
not count as detection. With the official EDUP DLL, the control must complete
creation successfully while EDUP selects the chain but reports neither device
attachment nor an active Present callback. JSON receipts include executable and
DLL hashes.

`proxy_chain_downstream_baseline_run` supplies a build that preserves the outer
proxy but loses the explicitly configured downstream proxy. Its three combined
chain cases must fail with device flags `0x20` instead of `0x21`: the outer BGRA
policy survives while the downstream single-threaded policy is missing. An
outer-proxy bypass cannot satisfy this control. The repaired build must retain
both policies and the downstream swap-chain buffer count.

## Regression sensitivity

The manual **Runtime regression checks** workflow accepts `mutation_checks=true`.
The 49 cases have 50 deliberate breaks: the expiring-alias case independently
breaks resource-creation metadata and reuse of the resolved destination. Mutation
labels distinguish those two runs while invoking the same native case.
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
