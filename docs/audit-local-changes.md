# EDHM local optimization and architecture audit

Audit date: 2026-11-12. Audited source: `34d92f27f588bbc3f72320c0fa17b674638698c6`.

This is the local-change appendix to the repository performance audit. It covers
EDHM changes relative to XXMI `v0.9.8`, plus the much narrower change after the
latest local stable release. Inherited original-3Dmigoto-to-XXMI work and newer
upstream releases are covered in the other audit appendices. Source statements
below describe inspected code; they are not measured FPS improvements.

## Brief

The most directly relevant local reductions in ordinary EDHM play are quieter
logging, less frequent buffered-log flushing, skipping fuzzy rules after a
successful exact hash match, and avoiding inactive advanced-hunting binding
queries. The local L2 region-key hash also repairs severe clustering for
equal-sized regions, but region tracking is off in the shipped play template.
These changes should not be given the same expected impact for every user.

Much of the remaining delta is compatibility and correctness: real D3D input
layouts with owned metadata, early include loading, Wine/DXVK chaining, safe
COM lifetimes, correct cache invalidation, and bounded binary/file parsing.
Several fixes deliberately add checks, copies, references, or locking. They
can improve stability while costing CPU time; an incorrect or crashing baseline
cannot establish an acceptable faster alternative.

No runtime source changed between local `v0.1.3` and audited HEAD. That range
only adds `-property:EdhmStrictBuild=true` to the Zip Release build command.
Its purpose is consistent C++17/strict compilation, not a demonstrated runtime
speedup. [Release diff](https://github.com/Fenris159/EDHM_2DMigoto/compare/8d4272e5beb0203470afd5bdc3644e1fda4c510e...34d92f27f588bbc3f72320c0fa17b674638698c6).

## Scope and method

| Reference | Immutable object | Meaning |
|---|---|---|
| EDHM audited HEAD | `34d92f27f588bbc3f72320c0fa17b674638698c6` | Current tracked source; untracked `addons/` is excluded |
| Latest local stable release, peeled commit | `8d4272e5beb0203470afd5bdc3644e1fda4c510e` | `v0.1.3`; annotated tag object itself is `90287c9aa66716a9f3250755c2adc9a5856c5d47` |
| Imported XXMI comparison | `f8e7118a1220e43546e546efa31892b8304358df` | XXMI `v0.9.8`; not a claim that this is latest upstream |

The audit inspected `git diff -w v0.9.8 HEAD`, the local runtime commit history,
introducing patches, active defaults, and current functions. Whitespace ignoring
does not eliminate all modernization noise: `NULL` to `nullptr`, explicit
casts, parameter names, constness, formatter rewrites, and new helper extraction
can still produce large diffs without an optimization. The XXMI 0.9.5–0.9.8
port is already in the `v0.9.8` baseline; it is not counted again as a new local
algorithm. Its EDHM adaptations are counted here.

## Optimization ledger

All wall-time, frame-time and FPS deltas in this appendix are **not measured**.
The main audit's profiling artifacts provide any separately executed component
measurements. The rate reductions below follow from source constants and are
operation-count calculations, not benchmark results.

| ID | Local change and before/after | Activation and expected affected work | Limits and separate measurement |
|---|---|---|---|
| L01 | `RunFrameActions`: change `fflush(LogFile)` every Present to at most approximately once a second, except debug mode | Open log, `debug=0`. At steady 60 Presents/s the scheduled flush calls fall from about 60/s to 1/s (about 98.3% fewer); at 144/s, about 99.3% fewer | An empty buffered stream need not produce disk writes; unbuffered logging already bypasses batching. Benchmark both variants with the same emitted bytes, buffered/unbuffered separately, on the actual target disk. Last buffered diagnostics may be lost on abrupt failure. [Implementation and introduction][L01] |
| L02 | Move successful shader/Texture3D creation and TextureOverride match chatter behind `LogDebug` | `debug=0`, most relevant at shader/resource creation or large match volumes | Saves formatting/serialization/output only where those messages would otherwise execute with an open log. Compare bytes, log calls, shader-loading duration and CPU samples independently of L01; enabling debug changes many other paths. [Logging changes][L02], [remaining creation-message cleanup][L03] |
| L03 | `Present1` no longer sets global `gLogDebug=true` on entry and `false` on exit | Applications actually using `Present1`; respects the user's debug setting | This also fixes cross-call/global diagnostic behavior. Do not attribute a `Present`-only run to this fix. Isolate the assignments in an otherwise identical DLL and compare Present1 traces/log volume. [Patch][L03] |
| L04 | EDHM theme signal polling changes from local 0.25-second interval to 2 seconds | Nonempty `auto_refresh_file_to_monitor`; sample enables it | At sufficient Present frequency the maximum steady poll rate falls from 4/s to 0.5/s: 87.5% fewer metadata calls. This is slower detection, up to roughly two seconds plus frame scheduling; the feature itself adds I/O relative to stock XXMI with no watcher. Compare 0.25 and 2 seconds with no theme changes, then measure theme-apply latency separately. [Patch][L01] |
| L05 | `find_texture_overrides_for_resource`: exact hash matches return before descriptor/fuzzy matching | Default `fuzzy_match_alongside_hash=false`; only successful hash matches benefit | Avoids fuzzy scan and associated descriptor matching; no benefit for misses. This changes which `filter_index` wins and is required for EDHM semantics. Toggle the setting with the same representative EDHM rules; verify colors before interpreting speed. [Compatibility patch][L05] |
| L06 | `RegionHashKeyHasherL2`: fold upper offset bits into low bits with `h ^= h >> 32` | Region L2 lookups, normally only when region/spatial hashing is used; sample `track_region_hashes=0` | Old `(uint64(offset)<<32)|size` put equal sizes in the same low-bit bucket under the map's power-of-two mask on ordinary small tables. The fold distributes offsets into those bucket bits; aligned offsets can still collide. Benchmark equal-size sequential/aligned/mixed keys at fixed occupancy and capacity; report hit/miss separately. [Introducing patch][L06], [current hasher][L07] |
| L07 | Multi-page version validation uses page versions plus one summary per 64 pages | Region hashing | This accelerates correct range validation relative to scanning every page. Upstream's starting-page-only check was cheaper but missed mutations elsewhere in a region. Do not label the new check a measured speedup over that incorrect check. Compare a correct full-page scan with block summaries using 1-page, boundary-crossing and large ranges. [Patch][L06] |
| L08 | `cf01a0e7` guards PS/OM binding refreshes with `AdvancedHuntingActive()` | Advanced hunting configured but Level 2 inactive; also avoids entering refresh bodies when entirely disabled | Prevents inactive `PSGetShaderResources` / `OMGetRenderTargets` queries and resource-hash/reference work. The active-state test itself takes the global critical section, so this is not zero runtime overhead. Compare disabled, configured/inactive, active, and active/locked states; count D3D getter calls. [Patch][L08], [state access][L09] |
| L09 | Advanced context registry uses ordered contiguous entries and binary search; cap/expiry bound growth | Only active Level 2 submissions; 512 default entries, parser clamps to 1–4096, lifetime 120 frames | A new diagnostic feature, not a speedup over no feature. Lookup is logarithmic comparisons; sorted insertion and expiry can move/scan linear numbers of large entries. Parent shader filtering avoids submitting unrelated contexts. Benchmark repeated known contexts, new contexts and expiry separately; track global-lock wait. [Registry implementation][L10], [settings][L09] |
| L10 | Skip nontrivial detach work when Windows is terminating the process | `DLL_PROCESS_DETACH` with nonnull `lpvReserved` | Avoids hook removal, TLS cleanup, I/O and persistent-settings writes under the loader lock. This is termination safety; no steady-state FPS claim. Dynamic unload retains cleanup. Time shutdown separately if relevant. [Patch][L11] |

L01, L02 and L03 interact: a removed message changes how much useful work a
flush performs. Their isolated deltas must not simply be added. L05 and L06
also benefit disjoint conditions: ordinary exact texture matching can execute
with region tracking off, while L06 requires the region-cache path.

## Architecture changes and behavioral contracts

### A01: Reduced build and deployment graph

The slim tree replaces the upstream solution with `EDHM_2DMigoto.sln` and builds
`DirectX11` plus `BinaryDecompiler` and `DirectXTK_Desktop_2017`. Shader
assembler/signature sources and the HLSL decompiler compile into the runtime;
the adjacent `d3dcompiler_47.dll` is a Microsoft redistributable, not a newly
built compiler proxy. Injector/InjectorLib, DirectXGI proxy, compiler proxy
projects, old shader tests, unused DirectXTK platforms/audio, and full PCRE2
source are omitted from the slim branch. Prebuilt PCRE2/Nektra libraries remain
linked. This is a maintenance/package/build-scope change; deleting unbuilt or
unused files cannot itself demonstrate faster game frames. The clean XXMI
mirror is maintained independently for selective ports. [Slim-tree commit][A01],
[build graph][A02].

### A02: Real input layouts with owned side-car metadata

XXMI `CreateInputLayout` returned `HackerInputLayout` to the caller. EDHM instead
returns the genuine `ID3D11InputLayout*`, attaches cached element descriptors,
semantic strings, layout hash and shader signature as private `IUnknown` data,
and resolves it with `HackerInputLayout::FromLayout`. The original D3D object
owns the side-car through `SetPrivateDataInterface`; callers retrieving metadata
must balance references. The side-car must not release the caller's original
Create reference. Context clears, command-list clears and teardown release
cached metadata. Override commands obtain metadata from the side-car and bind
`GetOrigInputLayout()`, including the ported VB format/offset commands. The
feature trades metadata creation/copy/reference work for pointer compatibility
and lifetime safety; it is not established as faster than a wrapper.
[Current implementation][A03], [creation boundary][A04], [ownership fix][L03],
[XXMI port adaptation][A05].

The required invariant for future upstream ports is observable: the game and
real D3D context receive real D3D layout pointers. Reintroducing wrapper returns
while accepting upstream layout features would break this boundary. Compare
location load, docking, `ClearState`, command-list execution and layout override
usage under the D3D debug layer in diagnostic runs; measure Release separately.

### A03: EDHM configuration compatibility at initialization and matching

`skip_early_includes_load` now defaults false and the template explicitly sets
zero. EDHM include-defined constants/TextureOverrides therefore exist during
initial initialization; the deferred initial `ReloadConfig` mechanism remains.
This can add startup parsing work compared with deferred loading and can repeat
parsing later; it solves availability, not an unconditional load-time win.
`fuzzy_match_alongside_hash` makes XXMI dual matching opt-in; default hash-first
semantics restore EDHM color selection. `stereo_params` parsing defaults to
`-1`, so decompiled declarations do not add the StereoParams register unless
configured. Theme auto-refresh is a signal-file mtime check in Present that
sets the reload-pending flag; the earlier reload check normally consumes it on
the next Present. It is not an OS file-watcher thread.
[Compatibility patch][L05], [current config parsing][A06], [Present lifecycle][A07].

### A04: Logging policy becomes independent of legacy `calls`

`enabled` opens a diagnostic log independently of `calls`; `crash` can also open
it. The template uses `enabled=1`, `calls=0`, `debug=0`, `unbuffered=0`,
`crash=0`. `max_size_mb=32` rotates an oversized previous log when config is
loaded/opened; this is not a continuous hard bound on bytes written during a
session. The crash path can opt into unbuffered logging. Last log lines and
runtime cost therefore depend on the complete policy, not `calls` alone.
Changing from an upstream setup with no open log to this diagnostic default
can add work even after message suppression and batching. [Policy patch][L01],
[current load policy][A06], [template defaults][A08].

### A05: Standard Wine/Proton chain with explicit compatibility profile

The new `WineCompat` module detects `wine_get_version` once through Win32
`INIT_ONCE`, with environment heuristics as fallback. Auto profile (`-1`) applies
under Wine; `0` disables it and `1` forces it. Its automatic changes are only
`load_library_redirect=0` and `check_foreground_window=false`. It preserves the
normal Wine-prefix/DXVK chain instead of requiring a renamed local DXVK proxy.
It reports paths/settings/override presence and renderer conflicts at startup;
it does not log raw environment-variable contents. Initialization delays remain
explicit troubleshooting options. This is platform compatibility and diagnostics;
any Wine-versus-native-Windows timing comparison changes the renderer/platform
as well as this module. [Module][A09], [introducing patch][A10],
[diagnostic policy correction][A11].

### A06: Serialized initialization and per-thread one-call redirect suppression

D3D11 loading now executes through `InitOnceExecuteOnce`, validates chained
module exports, guards optional exports, and initializes caller outputs before
forwarding. The one-call LoadLibrary redirect-suppression flag moves from global
state to the existing Win32 TLS structure. TLS is allocated before hooks become
active, with cleanup on installation failures. This avoids one thread consuming
another thread's suppression and removes unsafe concurrent initialization.
It can serialize startup and add TLS checks; no per-frame performance benefit
has been demonstrated. [Current D3D proxy][A12], [TLS and DllMain][A13],
[failure-path hardening][A14].

### A07: Correct region invalidation and safe CPU snapshots

The local cache now tracks the newest invalidation over the full requested
range, with 64-page summaries and 64-bit versions. Invalidation uses `size_t`
end offsets; range math is checked/saturated rather than wrapping. Changed
resources drop the per-frame L3 cache as needed; `Present1` clears it like
`Present`. The flat map has a minimum capacity of two and bounded missing-key
probes/erase search, avoiding full one-slot infinite loops. These must remain
even if a microbenchmark finds a branch cost. [Cache patch][L06],
[bounded-map patch][A15], [current cache lifecycle][A16], [Present1 lifecycle][A07].

`SetDataCache` deep-copies CPU-owned bytes rather than retaining memory that
becomes invalid after Unmap. The fallback readback copies mapped staging data
into a temporary and then into the snapshot; the temporary is freed after use.
This fixes ownership and a leak but currently entails two CPU copies on that
fallback. Shared per-context staging/readback buffer reuse is an inherited XXMI
port mechanism, not a separate EDHM invention. Readback `Map` can still stall
the GPU; reusing buffers does not make it asynchronous. Attaching release
trackers outside resource-cache locks avoids reentrant lifetime operations
inside those locks. [Deep-copy change][A17], [temporary ownership repair][A18],
[current fallback][A16], [tracker attachment][L06].

The spatial L2 key correction includes all four inputs through mixed offsets
and cell-size bits, and bounds/overflow checks accept exact-fit snapshots. It
prevents specific wrong reuse; it is not a proof that the compressed key is
collision-free. The `random()` call counter becomes atomic for deferred-context
use; that repairs a race and may add contention. [Port review fixes][A18].

### A08: Public DirectX proxy and COM contracts

The proxy supports context-only device creation requests, balances deferred
context ownership and rejected query references, resets mirrored state after
runtime state-clearing calls, and keeps injected depth views alive while bound.
Feature-level forcing uses caller-safe storage. Rasterizer scissor overrides
use local descriptor copies in both DESC and DESC1 paths. Supported WinRT
D3D11/DXGI interop exports are forwarded with `E_NOTIMPL` if absent; undocumented
D3DKMT shims are removed. Query tracking for asynchronous frame analysis is
synchronized. These are API/ownership fixes, not independent FPS optimizations.
[Context/resource hardening][A15], [DirectX contracts][L11],
[initial ownership fixes][L03].

### A09: Bounded file, binary, path, numeric and diagnostic processing

The hardening sequence validates DXBC section offsets/sizes, rejects impossible
or oversized shader files before allocation, bounds first-line/source scans,
checks reload timestamps/metadata, initializes failure outputs, handles null
error streams, and distinguishes C `FILE*` from Win32 handles. UTF-8 INI names
and include globs are converted explicitly; DLL-relative paths and directory
containment are bounded and path-aware. Include/ShaderFixes access failures
produce diagnostics instead of blind reads. Numeric configuration parsing
rejects nonfinite/invalid dimensions/counts and overflow. Long-lived timing uses
`GetTickCount64` instead of wrapping `GetTickCount`; derived `gTime` is still
float, so this is wrap safety rather than a claim of arbitrary-duration timing
precision. Crash handling ignores continuable diagnostic exceptions, uses
atomic exception accounting and does not resume the same faulting instruction
after crash diagnostics. [Failure-path patch][A14], [broad safety patch][A17],
[diagnostic/context patch][A15], [numeric helper][A19].

Extra validation can raise CPU costs on cold load, malformed input or fallback
paths. These paths should be measured for startup/reload latency and peak
allocation separately from steady rendering. A valid FPS comparison must keep
shader/configuration contents identical and successful on both variants.

### A10: Optional Level 2 draw-context hunting

The new split is `AdvancedHuntingContext` identity/state/report generation,
`AdvancedHunting` runtime/config/key coordination, and D3D binding tracking in
`HackerContext`; overlay and marking use existing hunting integration. Level 2
holds a parent PS/VS and distinguishes resource, auto or draw scope. It tracks
CPU-side identities without replaying draws or intentionally reading back GPU
data. Active mode queries effective D3D SRV/RT bindings after changes to account
for automatic unbinding, filters injected IniParams from identity, and excludes
deferred contexts because record-time and execution-time bindings differ.
Registry capacity/expiry and capture locking bound collection. Candidate INI
output is explicitly conservative because the INI language cannot express the
whole observed identity. [Feature introduction][A20], [runtime][L09],
[identity/state/report][L10], [binding capture][A21].

Default `advanced_hunting=0` avoids collection and inactive D3D binding queries,
but `AdvancedHuntingActive`/`Configured` lock-protected checks remain at call
sites. It is inaccurate to promise literally zero CPU overhead. Active mode can
scan 128 PS resource slots after output changes and take global locks on draws;
fingerprint/report/verbose overlay work can add further diagnostic overhead.
An independent profiling series must include disabled, configured-but-inactive,
active collection, cap saturation, capture-locked and overlay-verbose cases.
The existing focused tests cover identity, reports, lifecycle and deferred-context
policy; passing those is not evidence of a frame-time improvement.
[Idle-refresh fix][L08], [focused tests][A22].

### A11: Native quality gate and build hardening

Repo-local clang-format/clang-tidy, strict MSVC C++17 builds, source-controlled
warning/analysis policy, shader validation, pinned PowerShell analysis tools and
Sonar build capture/history reconciliation become maintained workflows. Mechanical
changes include explicit casts, fixed logging format strings, constness,
polymorphic destructor/ownership repairs, safer buffer helpers and bounded
parsers. Release/Zip Release configurations enable `/GS` stack-cookie protection;
post-build compiler redistribution uses the matching architecture. These changes
improve reviewability, diagnostics, ABI safety and build repeatability, and
stack protection can add runtime work. They do not establish faster output
than the same optimized compiler configuration before cleanup. [Build hardening][A23],
[quality baseline][A24], [strict settings][A25], [quality contract][A26].

The large September source diffs should not be treated as thousands of new
optimizations. `595e2fb3`'s const-preserving shader hash cast, for example, retains
the same hash algorithm. Replacing a `wstring(c_str())` reconstruction with an
ordinary string copy can avoid a length scan at parse time, but no isolated
measurement establishes a user-visible gain. [Const fix][A27], [ownership cleanup][A28].

## Separate attribution plan

For L01–L08, build two variants from the same full SHA and compiler/toolchain,
changing only the named behavior. Record the actual patch and binary hash.
Configuration switches can isolate L05 and feature activation; switches alone
cannot isolate a code change that changes many debug paths together. Keep
correctness prerequisites in both variants. For L07 compare two correct
range-validation algorithms; for L06 keep map capacity, occupancy and probe
bounds equal. Do not revert safe ownership to manufacture a faster baseline.

| Change group | Primary metric | Additional evidence required |
|---|---|---|
| Logging suppression/flush/Present1 | CPU time in wrapper/logging; frame-time median and tail; emitted bytes | Actual Present versus Present1 usage; open-log state; buffering and disk; no debug layer in performance run |
| Theme polling | Metadata-call count and CPU time; theme-apply latency | Stable signal file for overhead run; controlled mtime changes for latency run |
| Exact-before-fuzzy | Matching time and number of fuzzy evaluations | Same rules, hashes and draw counts; visual/color equivalence for EDHM |
| L2 hasher and bounded probes | ns/hit, ns/miss, load factor, collision/probe distribution | Equal-sized, aligned, mixed and adversarial keys; no end-to-end extrapolation from synthetic rate |
| Range validation | ns/lookup/invalidation for correct algorithms | Regions crossing page/block boundaries; mutation outside starting page must invalidate |
| Snapshot/COM/resource lifetime | Peak memory, allocation count, startup/reload time, long-session stability | Matching content; failure injection or D3D diagnostics in separate correctness runs |
| Advanced hunting | Getter calls, per-draw CPU time, lock contention, registry size, frame tails | Disabled/inactive/active/locked scenarios and identical draw stream; author-tool overhead reported separately |
| Slim tree/build/quality | Clean build duration, artifact size, diagnostic outcome | Same toolchain and code actually linked; no automatic FPS claim |

The runtime already has summary and command-list/command profiling modes using
`QueryPerformanceCounter`. These can locate wrapper costs and counts, but enabling
them adds instrumentation; they are not GPU-time measurements and cannot replace
an external frame-time trace. [Profiler interface][A29].

## Recommended local retention and evaluation order

1. Retain the real-layout boundary, classic EDHM include/matching semantics,
   correct cache invalidation, safe snapshots, bounded probes and COM/DirectX
   contracts as correctness prerequisites. Validate these before comparing speed.
2. Keep the quiet play profile and isolate L02, L03 and L01 in that order,
   recording logging interaction rather than adding percentages.
3. Keep the two-second theme interval; evaluate responsiveness against its small
   metadata-call reduction rather than treating it as a major FPS feature.
4. Keep advanced hunting disabled in ordinary play; quantify disabled checks
   before considering a lock-free or combined-state fast path. Such a change
   needs a concurrency design and evidence first.
5. For mods that actually enable region hashing, evaluate L06 before changing
   cache structures, then measure correct multi-page validation and snapshot
   fallback stalls. Leave tracking off when a mod does not require it.
6. Retain the slim build and quality tooling for maintenance. Evaluate newer
   upstream features in dependency order using the upstream appendix; do not
   replace this local safety/compatibility boundary wholesale.

## Source index

Every link is pinned to a commit or audited source, rather than a moving branch.
Function names in the text are the durable navigation anchors.

[L01]: https://github.com/Fenris159/EDHM_2DMigoto/commit/cba6ec07318b9fd377f19a86da3b3a3277bb768b
[L02]: https://github.com/Fenris159/EDHM_2DMigoto/commit/cba6ec07318b9fd377f19a86da3b3a3277bb768b
[L03]: https://github.com/Fenris159/EDHM_2DMigoto/commit/da6fc5aa9f9fe8120f9b055ba5f323853447f1ba
[L05]: https://github.com/Fenris159/EDHM_2DMigoto/commit/299a2e709f4a3e426be6048cc9e74287ba408ed7
[L06]: https://github.com/Fenris159/EDHM_2DMigoto/commit/6a1de86c0542ad866b923c374f862e3c87a1ac65
[L07]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/ResourceHash.h#L343
[L08]: https://github.com/Fenris159/EDHM_2DMigoto/commit/cf01a0e7c46a58864bcd365f5745bed1d49e4703
[L09]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/AdvancedHunting.cpp
[L10]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/AdvancedHuntingContext.cpp
[L11]: https://github.com/Fenris159/EDHM_2DMigoto/commit/6a1de86c0542ad866b923c374f862e3c87a1ac65
[A01]: https://github.com/Fenris159/EDHM_2DMigoto/commit/b16a44d1a2c0a41f27e21d2d5db10b6ab7c4d2af
[A02]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/EDHM_2DMigoto.sln
[A03]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/HackerInputLayout.cpp
[A04]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/HackerDevice.cpp#L1809
[A05]: https://github.com/Fenris159/EDHM_2DMigoto/commit/3b342b2361073404c1afa2e436eafd1f18e38401
[A06]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/IniHandler.cpp#L4909
[A07]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/HackerDXGI.cpp
[A08]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/Dependencies/d3dx.ini
[A09]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/WineCompat.cpp
[A10]: https://github.com/Fenris159/EDHM_2DMigoto/commit/905bbbab3130aabfc7bcb1190446e7b16202e974
[A11]: https://github.com/Fenris159/EDHM_2DMigoto/commit/5292d55f060c2863c4ea924d087d7e9eaeade512
[A12]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/D3D11Wrapper.cpp
[A13]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/DLLMainHook.cpp
[A14]: https://github.com/Fenris159/EDHM_2DMigoto/commit/fd92a50c56399ef3cfa3f8c253cf120adddd4760
[A15]: https://github.com/Fenris159/EDHM_2DMigoto/commit/ec5b7db32865124d11c4f7526d3b118d9533e366
[A16]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/ResourceHash.cpp
[A17]: https://github.com/Fenris159/EDHM_2DMigoto/commit/5c678bc068801d124c33a497d953ef92c5bb9aa3
[A18]: https://github.com/Fenris159/EDHM_2DMigoto/commit/24ebce13fe1b16fc577c0ab8f4e7886c6ccab108
[A19]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/numeric_parse.h
[A20]: https://github.com/Fenris159/EDHM_2DMigoto/commit/fc572f81796daecc76cc880d187d0864160d6377
[A21]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/HackerContext.cpp#L3462
[A22]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/AdvancedHuntingTests/AdvancedHuntingTests.cpp
[A23]: https://github.com/Fenris159/EDHM_2DMigoto/commit/b5e857fbc70724f4926cfc21ef0fe486a720cd9f
[A24]: https://github.com/Fenris159/EDHM_2DMigoto/commit/bf450a394b66ebc0522d99deac862eac6ef6f42c
[A25]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/Directory.Build.targets
[A26]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/docs/code-quality.md
[A27]: https://github.com/Fenris159/EDHM_2DMigoto/commit/595e2fb331d041e518f66ee869f692d185dc3ea8
[A28]: https://github.com/Fenris159/EDHM_2DMigoto/commit/73b077e9141002c932639bae43c6ed7dec39bd03
[A29]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/profiling.h
