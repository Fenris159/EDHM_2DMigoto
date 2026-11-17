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

The 64 cases also cover monitor-mode/window/reload transitions and recursive or
shared command counts; summary-only matching/view diagnostics; hash-first and
ordered dual matching; documented expression operators; all four view kinds,
resource/descriptor/raw/override fallbacks, changing `this` targets, warm-cache
metadata changes, custom/pipeline ownership, and failed-view binding preservation.
The PR #43 cases additionally protect smaller constant-buffer region copies,
clearing saved SRV-backed resources through writable views, runtime-learned pool
bind flags, scalar/local variable precedence over pool names, stable `this`
view-cache reuse with safe kind changes, and zero-frame reload measurements.
Stable-release review checks add deferred startup with unread persistent values,
changes made at the save-replacement boundary, byte-sized vertex copies,
integer clears through saved RTVs, and expiring STATIC/dynamic variable aliases.
The follow-up cases protect deferred batches across reload/wipe, a reload
waiting for an in-flight autosave, hexadecimal stencil clears, and input-triggered
reload during frame-analysis draw/update/explicit/deferred-playback dumps. Concurrency
uses events and OS lock/filesystem boundaries, with no sleeps or timing limits
in expected behavior. An unwritable timeout log must retain the original
timeout and its unsafe-cleanup marker.
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
The shutdown checks keep a tracked resource alive through a view, then require
its live metadata to disappear on final release. The retirement case destroys an
actual `Globals` owner and protects its storage against reads before releasing
the resource. It runs last in the all-cases executable because retirement is
irreversible; CI runs each case in its own process. Product-DLL child processes
also retain the actual private-data callback from a tracked buffer in an
earlier-loaded fixture. GPU objects are released before process termination;
the callback's final release occurs after product teardown and must return zero
before the fixture writes its completion receipt. Standalone, fixture-chain and
official EDUP 0.22.2 paths must exit cleanly; ordinary unload releases every
object first. Retaining whole WARP objects until process-exit CRT cleanup also
fails without EDHM loaded, so that backend behavior is excluded from this test.
See [the lifecycle rationale](../docs/resource-registry-shutdown.md).
CPU timing is never a regression threshold.

## IniParams publication preparation

The preliminary publication checks preserve existing upload points and protect
failure/retry ownership. The portable policy check runs without D3D:

```bash
c++ -std=c++17 -Wall -Wextra -Werror -pedantic RuntimeTests/IniParamsPublicationTests.cpp -o publication-tests
./publication-tests
python -m unittest discover -s RuntimeTests -p test_readiness_inventory.py -v
```

`IniParamsUploadTests.vcxproj` builds the concrete production D3D adapter check
for x64 and Win32. It verifies actual texture contents after failed Map, retry,
successive complete-image publication and resource recreation. Two offscreen
draws use different published images; one final readback requires the first
pixel to retain the first image and the second pixel to use the next image.
No readback or explicit flush separates those draws, and no swap chain or
Present runs. `--draw-only` selects that ordering case. Its default uses WARP;
`--allow-hardware` explicitly permits a
fallback for hosts where WARP is unavailable. Wine results remain distinct from
native Windows validation.

The production-path cases `iniparams-transition-retry` and
`iniparams-command-retry` additionally verify pending publication after a final
transition or command state retires. They use GPU readback and a controlled Map
failure; the transition clock is fixed. These new native cases are not accepted
as verified until their Windows run and mutation checks pass. Current evidence
and remaining gates live in the [pre-production status](../docs/pre-production-edo-readiness.md).

`iniparams-contexts` verifies that recording through a deferred context does not
acknowledge a failed immediate upload. The immediate retry publishes the newest
CPU image, and executing the deferred work retains its earlier recorded image.
The standalone adapter also requires wrong-capacity uploads to fail without
changing the prior texture contents. Inventory checks cover inherited uncertain
conditions and refusal to overwrite either an output or an input file.

`entry-path` additionally protects later population of ordinary/dynamic lists,
proxy retargeting and profiler counts. `benchmark-entries` measures the public
entry point with debug validation disabled; the existing `benchmark-lists`
mode uses preallocated state and measures a different scope. For a focused
native sensitivity run in a clean disposable checkout:

```bash
python RuntimeTests/check_mutations.py --case iniparams-transition-retry --case iniparams-command-retry --case entry-path
```

The manual runtime workflow accepts `mutation_case_filter` as a comma-separated
list. With `mechanism_benchmarks=true`, `entry_benchmark_baseline_ref` can select
an ancestor for a shared-harness A/B/B/A entry comparison. Leave it empty for
candidate-only measurements. It accepts only a comparison where CommandList.cpp
is the sole changed production file. Keep output types and source provenance
separate from the publication kernel probe in `IniParamsUploadTests`.

## D3D12 helper swap chains

`run_proxy_chain_tests.py --d3d12-only` exercises the actual DLL on Windows with
real D3D12 WARP queues. HWND, legacy and composition chains must expose
`IDXGISwapChain4` and retain the requested 32 by 24 dimensions and two buffers
with D3D11 width/height overrides enabled. The CoreWindow case uses an invalid
window and compares its HRESULT to native DXGI before loading the product.
Each API also checks null-device failure against native DXGI before loading
the product. None of these helper calls may report an unsupported DirectX version.
D3D11 buffer tracking must survive subsequent device/swap-chain creations in
the same process. The Windows runtime workflow runs these cases on both
architectures; they require D3D12 and do not present frames or run XeFG.
[Policy, reproduction and limits](../docs/d3d12-helper-swap-chains.md).

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

`ProxyFrameTests.vcxproj` and `FrameProxyFixture.dll` exercise the product DLL
when an outer proxy replaces Present/Present1 and suppresses native D3D11
presentation. GPU readback verifies single pre/post increments, zero-draw
frames, ignored buffer/test/foreign-thread/secondary-chain calls, failure
cleanup, normal presentation and recreation. Separate controlled-input cases
cover hotkeys, preset transitions and reload. See the
[frame-action repair](../docs/edup-proxy-frame-actions.md) for exact outcomes.
The runtime workflow builds both fixture configurations for x64 and Win32;
`run_proxy_frame_tests.py --case flow|controls|flow1|controls1` runs one process
per case. The x64 0.1.7 release control must fail on zero counters specifically.
The manual `proxy_frame_mutations` workflow input checks ten deliberate breaks
in frame dispatch, duplicate suppression, thread/chain ownership, input,
presets, transitions and reload. It restores source, performs a clean product
rebuild and reruns the protected cases using the existing tested restoration
and quarantine logic. These are finite off-game correctness checks, not a
presentation benchmark.

`ReloadNotificationTests.vcxproj` checks the actual product DLL's rendered
notification pixels after one off-game correctness Present. A sequential swap
chain retains the image for readback, as specified by
[DXGI_SWAP_EFFECT_SEQUENTIAL](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ne-dxgi-dxgi_swap_effect).
The three cases retain the default green confirmation, suppress it while the
image stays black, and retain orange warnings with the confirmation suppressed.
The fixture freezes only the product DLL's imported `GetTickCount64` clock at a
tick after initialization. The one Present therefore meets the initial reload
condition even on a warm startup, without waiting for real clock progress or
changing driver/OS clocks.
Each case also reads back the reloaded GPU constants and requires the reload
entry in `d3d11_log.txt`. The disabled setting first appears in the reloaded
file, protecting live changes to the option. Both architectures run these
checks; x64 also requires the pinned beta.2 DLL to fail the suppression cases on
the specific visible-pixel assertion. This is output validation, not a Present
loop benchmark or a live-game check.

The runner retains partial stdout/stderr and a failed timeout receipt before
re-raising a fixture timeout. Its process-boundary checks cover byte/text output
and an absent stream:

```bash
python -m unittest discover -s RuntimeTests -p test_reload_notification_runner.py -v
```

```powershell
msbuild RuntimeTests/ReloadNotificationTests.vcxproj -property:Configuration=Release -property:Platform=x64
python RuntimeTests/run_reload_notification_tests.py --dll x64/Release/d3d11.dll `
  --exe .quality/reload-notification/x64/ReloadNotificationTests.exe `
  --output .quality/reload-notification/manual-results
```

The manual **Runtime regression checks** workflow accepts `mutation_checks=true`.
The 64 cases have 66 deliberate breaks: the expiring-alias case independently
breaks resource-creation metadata and reuse of the resolved destination, and the
zero-frame case independently breaks command measurements while retaining their
registration. Mutation labels distinguish the runs invoking the same native case.
Its disposable checkout builds each deliberate behavior break, requires the
focused case to fail, restores the exact source bytes, then rebuilds and reruns
the restored cases. Local use requires a clean isolated worktree because the
runner temporarily rewrites the production command-list, configuration,
resource-matching and profiling sources. Timed-out builds run in an owned Windows
job (a process group on Linux); cleanup stops descendants before restoring source.
The restored build uses Rebuild so mutant objects cannot be reused. If cleanup
cannot establish a trusted executable, the runner removes it and preserves the
original failure. Python checks exercise the process boundary with a controlled
readiness handshake, including a real descendant, rather than waiting for time
to elapse.

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
