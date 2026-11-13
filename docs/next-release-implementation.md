# Next release implementation and validation report

**Date:** 2026-11-12. **Branch:** `upstream-improvements`.
**Integration base:** `34d92f27f588bbc3f72320c0fa17b674638698c6`.
**First-wave runtime:** `b14272fa74a3221092ccd6279e15a08463f864d6`.
The follow-up section records the newer profiler, parser and view candidate separately.

## Brief

The next-release candidate contains the small parser, resource-reference, pool
and GPU-readback repairs from the plan, plus the larger persistent-settings and
existing-buffer-region repairs. It also retains one measured startup improvement:
reusing the current INI section while parsing. Two other startup candidates were
implemented and evaluated in isolation, then deferred because their benefit was
not established for this EDHM configuration.

The section change reduced median configuration loading from **73.971 to
71.625 ms**, a **2.346 ms / 3.17%** reduction in an isolated snapshot of the Linux
EDHM installation under Wine. Load plus optimization fell from **74.792 to
72.484 ms**. These are CPU configuration measurements. No new FPS or GPU
frame-time improvement has been measured.

The code is under review in [PR #43](https://github.com/Fenris159/EDHM_2DMigoto/pull/43), targeting `develop`. The original dirty
checkout and installed game files were preserved. Strict builds, native regressions, mutation sensitivity and static analysis pass.
Game smoke and frame-time validation remain open.

The follow-up adds resource-matching and view diagnostics, once-per-window
profiler registration, and guarded compatible-view reuse. Native mechanism
measurements found 16–19% less enabled-profiler cost and about 91% less CPU cost
in a changing-buffer reference fixture. These are conditional CPU results.
Further parser changes were evaluated and left out after cumulative controls;
the approved regression follow-up below records the subsequent 29-case
validation and cache-metadata repair.

## Scope and references

The [execution plan](next-release-plan.md) defines the entry gates and ordering.
The [upstream intake audit](upstream-next-release.md), its
[XXMI appendix](upstream-xxmi-next-release.md), and
[original 3Dmigoto appendix](upstream-original-next-release.md) retain the source
comparison and dependency inventory. They describe the audit before
implementation; this report records what was actually selected and tested.

| Reference | Revision and purpose |
|---|---|
| Local source/release | Base `34d92f27`; v0.1.3 runtime source is equivalent to that base. |
| XXMI candidate selection | `065dc746c21c71ef48bbc93cb6b90b3dcdf766e6`, v1.2.3; `xxmi-base` remains a reference branch. |
| Actual prior selective-port cutoff | `326f64cf9ab565aa7cd3d50e002270994b8d16c6`. |
| Original 3Dmigoto | `8f329bd94fecc9bbcb9211ffd42a95dd7fe6b43e`. |
| Historical performance comparison | XXMI v0.9.8, `f8e7118a1220e43546e546efa31892b8304358df`; unchanged from the agreed audit baseline. |

No whole-branch upstream merge was used. Changes were adapted to the local APIs
and semantics, with separate commits for behavior and follow-up repairs. Version
numbers and release tags were not changed.

## Implemented changes and source provenance

Correctness fixes are justified by observable failures. Their timing contribution
is **unmeasured**, rather than zero or an inferred FPS gain.

| Plan item | Upstream source / local patch | Result |
|---|---|---|
| P1.1 empty groups | XXMI `c4e3a449`; local `1926bf81` | Reject empty or nested empty parentheses before iterator traversal; preserve valid grouping. |
| P1.2 save failure | Local `5ff4f8aa`, extended by `6664c64b` | Keep dirty settings and restore locale after a failed save; preserve the last complete file and stop unsafe reloads. |
| P1.3 numeric literals | `a9d7c89b`; `ff714f9e` | Accept leading-dot literals while retaining numeric validation and subtraction behavior. |
| P1.4 pool reset | `ad264db8`; `b989bee7` | Route a null pool assignment through the pool proxy-reset operation. |
| P1.5 CB stride | `6b2d803f`; `b333b2ed`, integrated into region repair | Clear structured metadata when recreating a constant buffer. |
| P2.1 reference flags | `038af009`; `973b0af3`, `b621ce9f` | Resolve multi-hop metadata requirements after parsing; handle CB aliases and rejected combinations consistently. |
| P2.2 pool metadata | `7d2c4705`; `818414ca` | Query template metadata without allocating or refreshing a lazy pool element. |
| P2.3 identifiers | `f9d665e2`; `8103a972` | Parse namespaced, Unicode and hyphenated pool identifiers before ordinary-variable fallback. |
| P3 structured `store` | `24e2cff7`, selected read-helper behavior from `1fba754a`; `cb04ad16` | Copy complete structure elements, validate source/index/range, use `memcpy`, and release owned views/resources. |
| P4.3 section reuse | `06962f17` + `8d156caa`; `dc015894` | Reuse the active section pointer for key/value insertion while preserving duplicate-section behavior. |
| P5.1 settings lifecycle | Final behavior from the XXMI persistence dependency inventory; `6664c64b` plus subsequent validation | One-reload grace, optional indefinite retention, filtering after `persist` removal, atomic file replacement and reload preflight. |
| P5.2 existing regions | `532a37c5` + `c226158e`; `81a36300`, `ed90fa48`, `b6651db7` | Treat size as byte count; validate backing bounds; create bounded views; distinguish CB copies from aligned CB references. |
| Additional bounds repair | Local `6b13d59f` | Reject unsupported typed-view formats without dividing by a zero element size. |
| Packaging | Local `141a4758` | Enable the missing Win32 Zip Release solution build mappings for the runtime and its dependencies. |

The [command reference](ini-command-reference.md) documents the user-visible
`store`, `Region` and persistence policies. `Dependencies/d3dx.ini` documents the
new `clear_unknown_settings` setting.

## Architecture changes and compatibility decisions

### Configuration-scoped resource metadata

Previously, resource-reference flags were propagated using the information
available at that point in section parsing. An alias whose final use appeared
later could leave its underlying resource without required bind flags.

The parser now records source/destination reference edges. It resolves those
edges to a fixed point after command-list parsing and clears them before
resource teardown and after propagation. This gives metadata a defined
configuration lifetime. It does not introduce a per-frame graph traversal.

The local `AddFlags` repair validates the combined bind flags before changing
either bind or miscellaneous flags. A destination with no flags can reference an
existing CB. A CB cannot acquire incompatible bind flags. Rejected combinations
leave both flag sets unchanged, which also prevents oscillation in cycles.
Structured/raw flags are removed when CB metadata is accepted.

Pool metadata reads use the template without resolving a runtime element.
Explicit static-element metadata remains supported. This prevents an
`unless_null` operation from extending a pool element's lifetime merely by
checking what would be needed for the destination.

### GPU readback ownership and bounds

`StoreCommand` now owns the source resource and source view through `ComPtr` for
all returns. The existing staging-buffer cache remains in use. The change adds
no CPU mirror or global write-tracking cache.

The offset remains a flat float index. It is checked for finiteness, sign and
range before integer conversion. Wide arithmetic determines the four requested
bytes and, for structured buffers, the enclosing complete structure elements.
The copy range must fit the source buffer. A successful map copies the selected
float into the destination variable; unsupported input, staging allocation
failure or map failure leaves its old value intact.

Native tests exercise real typed/structured data and view ownership. Staging
allocation failure is injected at the device boundary. Actual device-removal
`Map` failure has not been induced; its early return is inspected, and real
device-loss behavior remains an integration limitation.

### Explicit region extents

Region offset, byte count, underlying buffer extent, source CB visible window,
and new destination size are kept distinct. Bounds are checked before a region
operation changes destination state. Structured ranges must cover whole
structure elements. CB size rounding clamps first to avoid overflow.

View builders receive an exclusive extent; copy helpers receive a byte count.
An explicit reference range cannot reuse the full source view or a stale cached
view from a different range. The current correctness path recreates that
explicit-range view; descriptor-keyed reuse is a future optimization if actual
view-creation cost warrants it.

A copied CB uses ordinary CB binding. An explicit CB reference uses the ranged
binding contract, including 256-byte offset/count granularity and a maximum
64 KiB visible range. Invalid references preserve the old binding. Dynamic slot
ranges and stream-output expansion were not prerequisites for this repair.

### Persistent settings as a load/save lifecycle

`PersistentSettingsState.h` owns the current and previous unknown scalar sets
for one settings-file path. Full ordered names and float bit patterns determine
whether the unknown set was unchanged. This avoids count-only or lossy-hash
comparisons that can discard newly changed values.

The confirmed policy is:

- The first unknown set receives a reload grace period; a second identical load
  discards it by default. Changed names or values renew the grace.
- `clear_unknown_settings=0` retains unknown values indefinitely.
- If an existing global or pool variable loses `persist`, the old saved value is
  ignored and scheduled for removal. Locked global assignments remain ignored.
- If a missing variable returns during the grace period, its saved value can be
  restored through the ordinary assignment path.
- A successful explicit wipe clears both the file and retained state. A failed
  deletion retains state and the wipe request for retry.

Saving writes a temporary file in the destination directory, checks stream,
flush, commit and close results, then replaces the destination with
`MoveFileExW`. Only successful replacement clears the existing integer dirty
flags. A failed replacement preserves the prior file and pending values.
Locale restoration uses a noncopyable RAII guard.

`SavePersistentSettings` and `WipeUserConfig` return success/failure. Reload
preflight cancels a reload that cannot save or wipe, preserving live values; the
user can retry after resolving the write failure. Cleanup saves run after
Constants initialization. Existing sticky dirty-bit scheduling and theme reload
coordination are preserved. Atomic replacement concerns the file; this work
does not redesign all legacy global-variable synchronization.

### Active section reuse and validation infrastructure

INI parsing now passes the current section pointer through its stream/header/
key-value helpers. The pointer resets on each header and is taken from map
insertion results. Duplicate and recursive sections keep their existing rules;
UTF-8 paths, namespaces and include policies retain their existing semantics.

A small native test application imports the production source project through
conditional project properties. Normal DLL entry points are unaffected. Real
D3D WARP checks exercise pipeline/data contracts. A device-boundary adapter
records submitted buffer descriptors and can reject a staging allocation;
recording the submitted descriptor is necessary because WARP can normalize a
bad descriptor in the completed resource.

CI now runs focused cases separately on x64 and Win32 and can verify deliberate
behavior breaks. The strict package check actually builds Win32 Zip Release;
previously its solution mapping selected a configuration without enabling the
projects, producing no DLL. Manual analysis can restrict clang-tidy to compiled
files changed from an explicit base while retaining the Sonar build analysis.

## Isolated performance results

The input was a dereferenced snapshot of **107 configuration/shader files** from
the user's Linux Elite Dangerous + EDHM installation. The fixture reports seven
global variables and 816 Constants commands. Diagnostic logging, warnings,
hunting and `dump_usage` were disabled consistently in the snapshot. The installed
files were not edited. The snapshot digest and every tested source/binary hash
are in [startup-evaluations.json](benchmarks/startup-evaluations.json); observations
are in [startup-evaluations.csv](benchmarks/startup-evaluations.csv).

All variants use the same input directory and Wine prefix, Windows x64 MSVC
Release executables and Wine 10.0 on this Linux host. Each independent process
runs 11 cycles of **load → normal command-list optimization**. Sample zero is
reported separately; ten warm cycles per process enter the pooled medians. A
first-process load is not a cold-filesystem benchmark. No device or game frames
are timed.

| Isolated candidate | Processes per variant | Median load, before → after | Median load + optimize, before → after | Decision |
|---|---:|---:|---:|---|
| Current INI section reuse | 8 | 73.971 → 71.625 ms | 74.792 → 72.484 ms | Retain; 3.17% load reduction. |
| Constants compaction | 8 | 74.485 → 74.162 ms | 75.233 → 74.937 ms | Defer; effect below the experiment's resolution. |
| Static-condition memoization | 4 | 73.804 → 74.496 ms | 74.674 → 75.275 ms | Defer; no useful total benefit established. |

The section comparison's process-level median total reduction is 3.35%; a seeded
20,000-resample bootstrap gives a 95% **change** interval of **−4.54% to −1.94%**.
This supports the direction of the result within this fixture; it does not
account for every systematic Wine, scheduling or workload effect. Initial
four-process-per-variant observations also favored section reuse.

Constants compaction's process-level total reduction is 0.22%, with a change
interval of **−1.54% to +0.70%**. The current snapshot has only seven globals, so
its many command lines do not imply a declaration-heavy vector-erasure cost.
The evaluated implementation and ordering coverage remain in history, but
`b14272fa` restores the original declaration pass in the final code.

Static-condition memoization reduced the optimizer median by only **0.011 ms**
in the initial evaluation. Its total-time interval crosses zero. Per-If cache
state is not justified by that result. The independent evaluation branches
remain available for a workload that spends material time in this optimizer.

**Correction to the early constants figure:** the earlier 82.782 → 74.590 ms
parse-only result skipped normal optimizer cleanup between loads and used an
earlier whole-file baseline. It is superseded and must not be cited as a
compaction gain. Its raw records are retained and explicitly marked limited in
[constants-compaction.json](benchmarks/constants-compaction.json). The corrected
single-function comparison above determined the final decision.

The final selected runtime (`b14272fa`, with compaction removed) was then compared
against the correctness stack before startup optimizations (`1d78d5b0`). Across
eight fresh processes and 80 warm samples per variant, median load was
**74.044 → 71.806 ms**, a **2.237 ms / 3.02%** reduction. Median load plus
optimization was **74.830 → 72.624 ms**. Its process-level total-time change
interval was **−3.85% to −2.12%**. This confirms the selected optimization in the
final combined code; it does not isolate the combined correctness repairs
against the untouched release. Raw data and binary identities are in
[startup-cumulative.json](benchmarks/startup-cumulative.json) and
[startup-cumulative.csv](benchmarks/startup-cumulative.csv). Do not sum isolated
percentages or infer a frame-rate change from configuration work.

## Verification evidence

Final runtime source: `b14272fa74a3221092ccd6279e15a08463f864d6`.

| Gate | Evidence / state |
|---|---|
| Unchanged strict baseline | [43296386477](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43296386477): x64/Win32, shaders and quality checks passed before runtime edits. |
| Initial defect detection | [43296886425](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43296886425): original behavior exhibited the confirmed parser/settings failures. |
| Final strict Release + Zip Release | [43315053539](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43315053539): passed strict x64/Win32 Release and Zip Release, existing hunting checks, DirectXTK shaders, formatting, PowerShell and metadata checks. |
| Final native regression + sensitivity | [43315064363](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43315064363): all 20 cases passed on both architectures; all 20 compiled mutations were detected on x64, and restored production behavior passed. |
| Final Sonar / changed-source clang-tidy | [43316212003](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43316212003), `104646bd`: passed build-wrapper analysis, quality gate and changed-source clang-tidy; zero unresolved Sonar issues. The later script-only compiler-constant cleanup does not change runtime source. |
| Pinned formatting | clang-format 23.1.1 checked all eight changed native files against the integration base; passed. |
| Local scripts | Python compilation, mutation-anchor validation and `git diff --check` passed. Hardened benchmark arguments accepted valid inputs and rejected invalid output/ref values. |
| Installed EDHM / real game | Pending; not replaced or launched as a benchmark by this implementation task. |

The compiler path was Windows Server 2022 / Visual Studio 2022 v143, with
MSBuild 17.14.60. Artifact downloads from the recorded build produced these
SHA-256 identities:

| Binary | SHA-256 |
|---|---|
| Release x64 DLL | `42ee8952cfdf7f6edd88965275b919726e48b38770199c1fd3e56dddc7ffafd8` |
| Zip Release x64 DLL | `a47a6987a079d20df82fa4541fe5610b48afbef7a55c1d79bc0f8e15af18e836` |
| Zip Release Win32 DLL | `7b5169d57f6fb28c9bf3bce16f885cc2d5dba0056dbd728e19de7adde56a1e51` |
| Configuration benchmark executable | `b5daeae534a5c012bbe71387dabfc9e7ee64b64ace644b5cf5d7d0f887c65525` |

These workflow artifacts are retained for 14 days. A DLL's version string still
reports the existing release version; use its hash and source revision to
identify this candidate. No debug-layer limitation was reported in the final
native run.

The 20 focused cases cover expression groups/literals, pending/atomic saves,
settings lifecycle, pool reset/metadata/identifiers, reference order/cycles,
submitted CB stride, typed/structured store values/view ownership/allocation
failure, region copy/views/CB binding, Constants ordering, INI section rules and
static/dynamic If outcomes. D3D cases request the debug layer and print an explicit
limitation if unavailable. The mutation runner requires every deliberate break
to compile and then fail its named case; it restores exact source bytes and
reruns the original cases. A compile failure is not counted as detection.
See [RuntimeTests/README.md](../RuntimeTests/README.md) for reproduction.

## Deferred work and re-entry conditions

These items were assessed against their plan gates. They remain explicit future
work; implementing them without the required evidence would not complete those
gates.

| Plan item | Decision and reason | Evidence needed to proceed |
|---|---|---|
| P4.1 constants compaction | Evaluated, then removed; corrected EDHM effect is within noise. | A declaration-heavy actual configuration and useful isolated/cumulative gain. |
| P4.2 static If memoization | Evaluated on an independent branch; negligible optimizer benefit here. | A profile showing material repeated static evaluation and a useful total reduction. |
| P4.4 buffer-contamination early exit | Defer. Normal workload has region tracking off; an added `GetType` affects the off path too. | Buffer/texture tracking-on/off total-cost comparison, including locking/profiling exits. |
| P4.5 broader compatible-view reuse | Deferred in the first wave; a follow-up candidate is now implemented and measured below. | Approved follow-up native coverage is recorded below; installed-game evidence remains open. |
| P4.6 duplicate ShaderRegex disassembly | Defer. Installed lexical scan found no active ShaderRegex declarations. | A real patterned-miss authoring workload, identical replacements and cold first-use measurements. |
| Smaller parser dispatch/normalization | Follow-up evaluated masks, traversal and namespace construction; all remain excluded after controls. | Reproducible incremental gain in the final combined binary and grammar compatibility. |
| P5.3 stream-output targets | Defer. No intended EDHM SO-target workload established. | A mod using SO plus offset, append-position and reference-array ownership cases. |
| Phase 6 TextureOverride cache | Defer. No real frame profile establishes matching as the dominant cost. Upstream has semantic/invalidation hazards for EDHM. | Ordered equivalence with hash-first/dual/untracked/region paths; resolution/config/hash generations; lifetime/concurrency design; real frame benefit. |
| Phase 7 SRV batching | Defer. Lexical references do not establish executed eligible runs. Upstream sorting/failure behavior can reorder side effects or clear old bindings. | Conservative eligibility rules, failed-copy preservation, ordered binding/output equivalence and measured executed runs/API/frame cost before conditional expansion. |
| Phase 8 cache-first store | Defer. No active store workload or readback stalls identified in the installed scan. | Full write/copy/clear invalidation, memory/upload cost and real total-frame benefit. |
| Reflection/dynamic ranges/new syntax | Defer. No authoring requirement established; independent bounds/lifetime/concurrency work. | A concrete mod use case and separately agreed compatibility contract. |
| Original decompiler extension `f63fe792` | Defer. Incomplete raw-buffer/Texture1DArray support is a shader-authoring project, not an ordinary-play optimization. | Disassemble/decompile/recompile cases across affected registers/components, preserving local bounds guards. |
| Header hygiene / policy changes | Optional or out of scope. No required runtime fix found in original's remaining range. | A concrete build or behavior need; preserve EDHM namespace, timing, logging and input defaults. |
| Disabled-hunting lock removal | Defer. Reload/concurrency safety has priority over unmeasured lock removal. | Disabled-path profile plus a safe synchronization design. |

## Promotion order and remaining release work

1. Review the correctness commits and their final combined behavior. Preserve
   the local hash-first matching, real input-layout pointers, side-car ownership,
   early includes, Wine/DXVK chaining, quiet defaults and region-cache safety.
2. Complete the native/analysis/package gate for the recorded runtime source.
   Review the settings policy as a user-visible behavior change independently
   of the small crash/type/ownership repairs.
3. Keep section reuse only while the final cumulative configuration result and
   compatibility checks remain favorable. Leave the other two startup
   evaluations out of the next-release runtime.
4. Before promotion, test the candidate in an isolated/recoverable Elite + EDHM
   installation: startup and HUD colors, location load, docking, F11 reload,
   theme auto-refresh, exercised `ClearState`/layout paths, settings restoration,
   and shutdown. Record Linux/Proton and Windows separately.
5. Capture a repeatable scene with controlled graphics/resolution, disabled
   diagnostics and alternating binary runs. Report median and tail frame times
   and wrapper costs before opening the cache/batching gates. The existing
   installed log contains no usable frame-time capture.
6. Only after those gates pass, choose the release version, update release
   notes and promote through the normal release process. This branch does not
   install a DLL, merge main or publish a release.

All **108** installed baseline files were rehashed after implementation and matched
the recorded originals. The Linux installed baseline DLL SHA-256 is
`d1fca1312e4435634148c0e46341ac7306ac0f4e2c421d4788262631cbf214c2`.
The Windows desktop CIFS mounts were unavailable during the audit. Native Windows
CI supplies API/build evidence, not evidence of a Windows Elite play session.

## Follow-up profiling, parser and view work

**Second-wave snapshot:** `83b4649200036ff9f8453763f14c97d0024554c8`, based on
`afe3372b`. The existing 20 native cases and their 20 deliberate-break checks
pass for `83b46492`, as do strict x64/Win32 Release and Zip Release builds.
The selected candidate removes the unproven operator-mask optimization;
validation evidence is recorded below. New targeted
regression cases were awaiting the behavior confirmation required by the
user's AGENTS.md at that snapshot. Approval and subsequent checks are recorded below. The installed-game smoke/frame-time gate remains open.

### Selected changes

| Change | Source and architecture | Measurement / scope |
|---|---|---|
| Separate resource/fuzzy matching diagnostics | Adapt XXMI `9b3d52f2`; local `2f4fc570`, `aa185f1e`, `d99c3462`. Add resource-matching CPU time, fuzzy scan CPU/count/hits, tested candidates and command-resource view-factory creation/reuse diagnostics. Collect new metrics only in summary mode; candidate counts are accumulated once per scan. Hash-first matching and opt-in dual matching retain their old condition. | Instrumentation makes the next game capture useful; it is not a measured game-speed improvement. Counts cover whole collection windows. Creation counts cover calls through the command-resource view factory, rather than all application/driver views or necessarily distinct objects. |
| Once-per-window profiler registration | XXMI `a6b30761`; local `be828781`. Per-command/list 64-bit generation markers avoid inserting the same identity into profiling sets on every execution. Configuration loading clears collections before replacing command storage; existing recursive timing accounting remains. | Synthetic native interpreter cost fell about 16–19% with profiling enabled; the profiling-off control was within noise. Each object gains an eight-byte generation marker, subject to alignment. Existing command-storage lifetime and synchronization assumptions remain. |
| Compatible source-view reuse | XXMI `ace1f29f`, adapted in local `88254a3a`, `9b71e9d4`, `fd95b056`, `1c6e4658`. Separate descriptor construction from view creation; require the selected resource, queried view interface and requested descriptor to agree. Hold the actual queried interface through binding/cache ownership. Preserve the ordinary warm cache path, exclude explicit regions, and respect destination format/stride overrides. Failed creation of a required pipeline view preserves the old binding. | Native WARP buffer-SRV reference benchmark: changing resources fell from about 1,364 to 117 ns/reference, with 10,000 → 0 Create*View calls per batch. Stable-resource medians were 51.87 → 53.59 ns/reference (profiling-off interval crosses zero). These are conditional CPU mechanism results, not a general frame-rate gain. |

`ComPtr` retains the queried interface instead of releasing it and casting the
original interface pointer. The resource check balances the reference returned
by `GetResource`. Descriptors are compared by their active dimension fields;
padding and unused union fields cannot affect the result. Planar-video resources
use the existing factory because a base descriptor cannot prove that the newer
`PlaneSlice` field agrees. Full-mip SRV descriptors are canonicalized only for
comparison; factory parameters retain their original values. See Microsoft's
[QueryInterface rules](https://learn.microsoft.com/en-us/windows/win32/com/rules-for-implementing-queryinterface),
[GetResource ownership contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11view-getresource),
and [PlaneSlice definition](https://learn.microsoft.com/en-us/windows/win32/api/d3d11_3/ns-d3d11_3-d3d11_tex2d_srv1).

At this snapshot, the ordinary warm-cache path still keyed reuse by resource
identity. The approved follow-up below adds targeted coverage and repairs that
metadata invalidation gap; the source-view comparison shown here runs when no
usable cached view is present. The timing fixture uses
buffer SRVs. It does not establish a speedup for textures, RTVs, DSVs or UAVs.

### Independent evaluations and uncertainty

The parser comparisons reused the same 107-file snapshot, path, Wine prefix and
load → optimize lifecycle used above. Each process performed 11 cycles; sample
zero was excluded from warm statistics. Source/binary identities, raw data and
seeded process-level bootstrap results are preserved in
[second-wave-parser.json](benchmarks/second-wave-parser.json) and
[second-wave-parser.csv](benchmarks/second-wave-parser.csv).

- Operator masks (`1ff00d94`, evaluated in `546fa856`) improved isolated
  process-median load-plus-optimizer time by 6.15% in
  the first screen and 7.72% in confirmation, with four processes per variant in
  each experiment. Confirmation's 95% change interval was −9.45% to −6.84%.
  They were then removed in `83b46492`: the gain did not survive the final
  combination. A same-source control without masks loaded in 73.226 ms versus
  74.299 ms with masks. Total-time change was +1.22% with a −1.48% to +3.87%
  process-bootstrap interval. No useful incremental benefit was established.
- Direct traversal (`87fa9479`) alone increased total time by 10.54% in this
  fixture. With masks already enabled, eight processes per variant found only
  a 0.29% process-median total reduction, with a −1.74% to +1.19% interval.
  It remains excluded.
- The conservative namespace-key construction variant, adapted from
  `ec9e5713` / `5c77f4b0` while preserving locale-aware Unicode folding,
  increased process-median total time by 8.16%. It remains excluded. A source
  change that removes operations is not sufficient evidence of a faster binary.

The native mechanism comparisons alternated fresh A/B processes in `ABBABAAB`
order on the same Windows runner. Each variant had four processes and ten warm
samples per mode/workload after excluding sample zero. Timing is not used as a
unit-test threshold.

| Profiler mode | Before → after, median process ns/command-list run | Reduction |
|---|---:|---:|
| Off | 20.684 → 20.735 | Within noise |
| Summary | 93.400 → 77.947 | 16.54% |
| Top command lists | 93.027 → 77.997 | 16.16% |
| Top commands | 95.742 → 77.124 | 19.45% |

These interpreter runs execute real assignments across 64 lists. They measure
profiler overhead under that workload, not the cost of a full EDHM frame.
[Raw observations](benchmarks/profiling-registration.csv) and
[identities/statistics](benchmarks/profiling-registration.json) preserve the
baseline that differs only in repeated-versus-generation registration.

The view comparison used real WARP resources with the debug layer disabled,
measuring stable references and round-robin changes among 32 buffers. In the
final profiling-off comparison, changing-resource CPU cost fell by 91.44%
(1,364.148 → 116.780 ns/reference). Stable references measured
51.870 → 53.590 ns/reference; the 95% process-bootstrap change interval was
−1.13% to +7.28%. With summary profiling enabled, the stable case rose
51.928 → 53.368 ns/reference (2.77%, interval +2.21% to +4.34%). An earlier confirmation
had similar stable medians but one candidate-process outlier. Stable batches
are short (10,000 references), and the final summary-mode result has a small
absolute penalty; no stable-path speedup is claimed. The saving depends on how often real commands
would otherwise create compatible views; the new diagnostics can measure that.
[Final view data](benchmarks/compatible-view-reuse.json),
[raw CSV](benchmarks/compatible-view-reuse.csv),
[the earlier confirmation](benchmarks/compatible-view-reuse-screen.json),
[the pre-field-comparison retry](benchmarks/compatible-view-reuse-retry.json), and
[the field-comparison candidate](benchmarks/compatible-view-reuse-field-comparison.json)
retain earlier observations as well as the selected result. The final runner recorded Windows build 20348, four logical
processors and AMD family 25/model 17; its exact image and executable hashes are
in the JSON.

The earlier `a2dc88a9` combination loaded in 71.591 → 66.887 ms versus
the first-wave runtime, but that result did not survive the active-field
descriptor correction: the `1c6e4658` comparison was 71.976 → 72.602 ms.
A second comparison using both an independently built and a mutation-restored
`1c6e4658` executable reproduced the absence of a useful cumulative gain, while
`a2dc88a9` remained faster in the same interleaved run. This establishes binary
sensitivity; the precise compiler/layout cause was not isolated. The descriptor
correctness repair is retained, and the operator-mask change is deferred after
the separate mask control. The selected `83b46492` build loaded in
72.032 → 72.090 ms against the first-wave runtime. Load plus optimizer was
within noise; no additional startup gain is claimed. [Raw controls](benchmarks/second-wave-cumulative.csv) and
[source/binary identities with uncertainty](benchmarks/second-wave-cumulative.json)
retain the superseded and selected observations. Isolated percentages must not
be added to the first-wave 3% result.

### Follow-up validation and remaining work

| Gate | Evidence / state |
|---|---|
| Existing native cases, both architectures | [43330895159](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43330895159): all 20 cases passed. |
| Existing mutation sensitivity | [43330915876](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43330915876): all 20 approved behavior mutations detected, restored production passed. |
| Strict Release/Zip Release and shaders | [43330935594](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43330935594): passed x64/Win32, shaders and quality checks. |
| Profiler-only analysis | [43323313875](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43323313875): passed Sonar/clang-tidy. |
| Combined candidate analysis | [43330895111](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43330895111): Sonar quality gate and clang-tidy passed against all native source changed since `afe3372b`. Descriptor-comparison findings were fixed without suppressions; the redundant operator-mask condition was removed with that optimization. |
| New targeted cases | This snapshot preceded the required behavior approval. Both recommended lists were subsequently approved; the 29-case follow-up is recorded below. The selected source makes no further grammar change. |
| Actual Elite frame outcomes | Pending. Steam was running when checked, but Elite was not; existing captures contained no usable frame-time trace. No DLL was installed or game files edited. |

The [CPU monitor instructions](ini-command-reference.md#cpu-performance-monitor)
explain Ctrl+F9/Shift+F9 captures. Those bindings work with hunting disabled.
Keep normal-play settings for outcome captures and use a separate summary-mode
capture to attribute wrapper work. The
[benchmark instructions](../RuntimeTests/README.md#interpreter-and-resource-view-measurements)
explain the synthetic native comparisons and their limits. TextureOverride
caching and SRV batching still require real workload counts and frame profiles
before their larger implementation gates open.

## Approved regression follow-up and cache validation repair

The user selected the recommended behavior cases for both prompts and explicitly
allowed separate test and benchmark branches. The original checkout remains on
`chore/vendor-edhm-runtime-v0.1.3`. Selected runtime work is collected on
`upstream-improvements`; `follow-up-regression-checks` preserves the approved
regression follow-up and its cache repair. The validated endpoint is collected
on the integration branch. The other parser/view/profiler branches retain
isolated experiments and controls. No release or game installation was promoted.

**Validated native source:** `9d93563fb27fd6eca201247689d03c8483894147`;
QA-runner correction: `c9a55a0a` (no native-source changes).
The existing 20 cases retain their expectations. Nine approved follow-up cases
bring the native suite to 29:

| Case | Observable result protected |
|---|---|
| `expression-grammar` | Documented precedence, associativity, unary/functions, grouping, malformed input and live namespaced variables. Existing Unicode/pool identifier checks also remain. |
| `profiling-collections` | Actual monitor mode cycles, warning-mode eligibility, fresh windows, recursive/repeated list counts and shared pre/post command counts. |
| `profiling-reload` | Old command collections and display text disappear before a new configuration executes. |
| `matching-diagnostics` | Hash-first matching, ordered dual/descriptor results, unmatched resources, summary-only counters/timers and clear behavior. |
| `view-reuse` | SRV/RTV/DSV/UAV bindings retain the selected resource, reuse source views, distinguish source/cache counters by mode, preserve mip ranges and release owned interfaces. |
| `view-descriptors` | Wrong resource/interface/range/format/raw flags use the correct fallback; changing `this` targets keeps the required kind; custom overrides affect the eventual pipeline view. Existing explicit-region cases remain. |
| `view-cache-metadata` | One operation updates a view when range or format changes on the same backing resource. |
| `view-ownership` | Repeated custom references keep the destination view alive and release it when its last owner disappears. |
| `view-failure` | Injected Create*View failures preserve existing bindings for all four kinds; success/failure counts follow the selected profiling mode. |

These are production parser/interpreter paths and real WARP bindings. The device
boundary injects allocation failures and records view calls; domain behavior is
not mocked. Expected arithmetic values and buffer ranges are hand-computed
literals. CPU time is never a positive-duration or performance pass/fail threshold.

### Failure found and repaired

The first run compiled successfully on both architectures and reproduced a warm
cache failure: a reference first requested offset 0 / extent 32, then offset 8 /
extent 24 on the same 32-byte float buffer. The second binding must expose
`FirstElement=2, NumElements=4`; the old cache returned the first view's `0,8`.
The subsequent format change must also produce an unsigned-integer view.
[The initial failing run](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43336169033)
and [the repeat with additional cases](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43336538499)
preserve that pre-fix evidence.

The cache previously checked resource identity alone. `4fc78ab4` adds an optional,
operation-owned record of stride, offset, extent, format, effective target kind
and raw-view mode. Reuse requires those parameters and the backing resource to
match. Copy operations and explicit no-cache cleanup invalidate the record.
Shared custom-resource views are checked against their active descriptors,
because a key belonging to only one command cannot describe updates from other
commands. The record owns no additional COM references.

A separate early test incorrectly required an untyped custom destination to have
a view immediately. Custom resources can hold override metadata until a concrete
pipeline target requests its interface. That test now observes the resulting
pipeline binding and still requires the requested UINT format, eight elements
and stride metadata. All raw-view fallback examples passed; no raw-view factory
repair was needed. No existing regression expectation was weakened.

Monitor mode policy now lives in `Profiling::next_mode`; the hunting key callback
delegates to it. This preserves the cycle and reset behavior while allowing the
checks to exercise the real transition without synthesizing input events or
using a real clock. The cases pass after the repair on
[both architectures](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43337051034).

### Final verification and measurement

Strict Release/Zip builds and shader validation pass for the repaired native
sources in [43337495559](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43337495559).
The 29-case mutation run uses the QA-only runner correction `c9a55a0a`; its native
sources and test cases are identical to `9d93563f`. A compile-rejected deliberate
break in an earlier run was not counted as detection; the replacement keeps the
same behavior defect while compiling under strict warnings.

The repaired runtime was compared with the pre-reuse mechanism baseline on one
Windows runner in [43338093659](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43338093659).
Changing-buffer references measured **1,416.690 → 122.295 ns/reference** with
profiling off (91.37% lower CPU cost, 95% change interval −91.94% to −91.09%).
Create*View calls remained **10,000 → 0** per changing-resource batch.
Stable references measured **53.238 → 56.698 ns/reference**; one baseline process
was an outlier, and the interval was broad (−28.79% to +10.59%). With summary
profiling, the stable case measured **54.120 → 57.405 ns/reference**, a 3.285 ns
penalty (+6.07%, interval +4.85% to +9.55%). No stable-path gain is claimed.
[Exact identities/statistics](benchmarks/compatible-view-reuse-final.json) and
[raw observations](benchmarks/compatible-view-reuse-final.csv) retain this final
native-code comparison separately from the earlier snapshot results.

The same 107-file configuration snapshot was also measured in twelve interleaved
processes, four per binary. Median loading was 71.109 ms for the first-wave
baseline, 72.140 ms for the earlier selected `83b46492` build and 70.869 ms for the
repaired `9d93563f` build. Total load-plus-optimizer change intervals cross zero
against both controls (−1.18% to +4.82% and −2.92% to +3.05%, respectively).
No additional startup gain is established. See
[configuration identities/statistics](benchmarks/follow-up-configuration.json)
and [raw cycles](benchmarks/follow-up-configuration.csv).

A separate same-runner comparison against the earlier selected `83b46492`
build measured the incremental validation cost. With profiling off, stable
references measured 63.740 → 68.225 ns/reference (+4.485 ns), with a wide interval
from process outliers; changing references measured 145.265 → 159.338 ns/reference
(+14.073 ns, +9.69%, interval +8.23% to +11.91%). Summary-mode stable references
measured 62.815 → 67.740 ns/reference (+4.925 ns, +7.84%, interval +5.24% to +9.76%).
Both variants already reused source views, so this is a correctness cost compared
with that earlier selected build. It is retained to prevent stale ranges/formats.
Do not combine percentages across the separate runner comparisons. See the
[incremental cost identities/statistics](benchmarks/view-cache-validation-cost.json)
and [raw observations](benchmarks/view-cache-validation-cost.csv).

| Final gate | Result and exact evidence |
|---|---|
| Native cases, x64 and Win32 | All 29 passed in [43338600288](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43338600288). |
| Mutation sensitivity and restoration | All 29 deliberate breaks compiled and were detected; restored production passed all 29. [Receipt](benchmarks/follow-up-regression-mutations.json) preserves source hashes and exits. |
| Strict Release/Zip and shaders | [43337495559](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43337495559) passed both architectures and quality checks. |
| Sonar and clang-tidy | [43337495572](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43337495572) passed; changed-source clang-tidy emitted no warnings. |
| Same-runner measurements | [Final view comparison](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43338093659) and [incremental validation comparison](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43338600288) passed; timings remain observations rather than regression thresholds. |

The earlier performance figures remain pinned to their original binaries. Both
behavior confirmations are resolved. Installed Elite smoke and repeatable
frame-time validation remain open; the installed game has not been modified.


## PR #43 review follow-up

Kilo reviewed `dbe8602734a09b24e8cfbed255bcb2091e8f1625` and reported nine
findings. All original PR checks were green. Eight findings reproduced; the
formatter prediction was a false positive. The user approved the six native
regression cases and the three mutation-cleanup failure cases before they were
written. Repairs were prepared on `pr43-review-fixes` before collection into
`upstream-improvements`; neither `develop` nor `main` was merged or advanced.

**Repair source:** `e9706f912a271d179b05c46f5df0276b589c7add`.
Documentation checkpoint `a516969c` preserves this native source and test snapshot;
the subsequent analysis corrections are recorded below.

| Finding | Verified outcome and repair |
|---|---|
| Ordinary builds require C++17 | Default Release and Zip Release failed with `std::optional` unavailable. Move the language standard and compatibility definitions outside the strict-warning condition in `Directory.Build.targets`. CI now builds both ordinary x64 configurations as well as the existing strict x64/Win32 configurations. |
| Smaller constant-buffer regions | A 256-byte copy from a 1,024-byte CB copied zero bytes after fallback stride inference. Use the explicit byte count as the fallback stride; preserve real structured/typed strides and backing-buffer bounds. New coverage checks exact contents at offsets 0 and 256. |
| Clear after retained SRV | A saved SRV prevented writable-view creation, leaving texture contents unchanged. Clearing reports whether a retained view was writable and falls back to creating an appropriate view when necessary. Readback checks both RTV- and UAV-capable textures. |
| Learned pool bind flags | A ring-pool copy lost the selected element's runtime-learned SRV flag and failed to rebind copied data. Add side-effect-free metadata lookup of existing elements, including proxy and keyed/spatial lookup. Metadata queries do not allocate, refresh expiration, switch types or re-key slots. |
| Scalar precedence | A pool named `PoolFoo` shadowed global/local `$poolfoo` scalars. Limit the early pool-variable parser to indexed syntax. Literal expectations protect local 11, global 7 and indexed pool value 2. |
| Stable `this` view cache | Three references without a source view created three views. Permit a warm `this` cache when the resource and complete request, including effective kind, match. Coverage requires one initial view and one additional replacement on an SRV-to-UAV change. |
| Zero-frame reload profiling | The first same-frame overlay update discarded initialization counts before any report. Preserve the collection when no frame has completed, while retaining the existing stale-pointer cleanup before reload. Coverage exercises summary, top-list and top-command modes without timing thresholds. |
| Mutation failure cleanup | Undetected mutations and failed mutant builds restored source bytes but left mutant executables. Rebuild and verify production in failure-safe cleanup; remove the executable if restoration fails and retain the original failure. Failed CI jobs upload diagnostic evidence without a native executable. |
| Claimed formatting failure | No repair required. The original file passes the pinned clang-format **23.1.1** with `--dry-run --Werror`, as does the actual PR quality job. System clang-format 21.1.8 gives different alignment output and is not the repository's pinned tool. |

[The red native run](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43357768952)
built successfully on x64 and Win32, passed the existing 29 cases, and failed all
six new cases for their intended symptoms. [The ordinary-build reproductions](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43357772151)
failed both configurations while the strict build passed. These failures are on
the isolated review branch and intentionally preserve the pre-fix evidence.

The cleanup cases exercise the actual runner with real temporary source/artifact
files; only the compiler/process boundary is replaced. All three assertions
failed against the old runner. Reverting only the cleanup repair made all three
fail again; restoring exact fixed bytes made all three pass. The native
mutation list includes the six new cases, bringing its total to 35.

Validation runs for the repaired snapshot:

- [35 native cases and deliberate-break checks](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43358505966).
- [Ordinary/strict Release and strict Zip Release builds](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43358509219).
- [Sonar and changed-source clang-tidy](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43358512269).

All 35 native cases passed on both architectures. All 35 deliberate breaks
compiled and were detected; the restored production build then passed all 35.
The [receipt](benchmarks/pr43-review-mutations.json) records the native executable
hash and per-case source hashes/exits. Ordinary and strict Release/Zip Release
builds also passed. The linked analysis run records the separate Sonar/clang-tidy
gate. No new performance percentage is attributed to these correctness repairs. Earlier
measurements retain their original source/binary identities. Installed-game
smoke and frame-time validation remain open.


### Analysis corrections after the first repair checkpoint

Sonar identified two possible null dereferences when a custom-resource metadata
lookup returns no object, and a cognitive-complexity finding in mutation cleanup.
`8535e8af` returns zero bind/misc flags for missing metadata and extracts the
failure-safe restoration routine. No analyzer rules or severities were changed.
The approved metadata coverage includes the missing-target boundary; the suite
now contains **36** native cases and matching deliberate breaks. The previous
35-case receipt remains pinned to `e9706f91` and is not relabeled as new evidence.
The existing three cleanup cases still pass after the extraction.


### Nested destination-index correction

Kilo's incremental review of `660b78fb` confirmed the earlier repairs and
reported [one new warning](https://github.com/Fenris159/EDHM_2DMigoto/pull/43#discussion_r4559421827):
`PeekResource()` is non-mutating, but its index expression can read another pool
and refresh expiration before an empty-source reference is skipped.

The first candidate, `0adaa0d6`, skipped unused destination-flag lookup for
existing caches and unconfigured empty sources. The final repair preserves the
flag learning that even an initially empty source can need when populated later.
A scoped metadata-read mode in `CommandListState` flows through resource,
variable, proxy and last-frame pool lookups. It previews existing/default values
without slot allocation, expiration refresh, type switching or re-keying, and
restores ordinary read behavior when expression evaluation ends. Existing caches
still bypass creation metadata they cannot use. Configured first-use sources
receive the selected destination's learned flags. The pool regression
also verifies that a new configured buffer can be referenced into an already
learned SRV slot and expose the literal values 11, 13, 17 and 19.

The first nested-index harness attempt lacked the command-list scope required
for a resource operand and stopped at parsing. Those failures were **not**
counted as reproduction. `193b687e` supplies the missing fixture scope without
changing the expiration expectation. The corrected
[control run](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43363009915)
reverts only the source-creation guard and fails the intended expiration check
on both architectures; the other cases pass. The fixed
[37-case run](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43362869804)
passes both architectures and records deliberate-break/restoration verification.

The earlier guard candidate and corrected harness are pinned to `0adaa0d6`
and `193b687e`. The scoped metadata-read repair is in `4575c2e3`, with typed
future-population coverage and an empty-pool guard in `4b04a2fb`.
Final validation of `4b04a2fb7c260e908f01fe93e3028a186fc7f650`:

| Gate | Result and evidence |
|---|---|
| Native behavior | All 37 cases passed on x64 and Win32 in [43364493847](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43364493847). The three mutation-cleanup failure cases also passed on both architectures. |
| Regression sensitivity | All 37 deliberate breaks compiled and were detected on x64; the restored production build passed all 37 cases. The [new receipt](benchmarks/pr43-nested-metadata-mutations.json) records source/test identities, the executable SHA-256 and per-case hashes/exits. |
| Builds and packaging | Ordinary x64 Release/Zip Release, strict x64/Win32 Release/Zip Release, formatting, script and shader checks passed in [43364500146](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43364500146). |
| Analysis | [43364496610](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43364496610) records the Sonar quality gate and changed-source clang-tidy against `origin/develop`. No analyzer rules were suppressed. |

The 35-case receipt stays pinned to its original source; no performance percentage
is relabeled or added for these correctness repairs. The repaired source is
collected on `upstream-improvements` for PR #43, targeting `develop`. Latest PR
checks and Kilo's incremental review are separate gates on the PR head.
Installed Elite + EDHM smoke and frame-time validation remain open.

### Detached pool lookup correction

Kilo's incremental review of `c1256952` reported four further warnings:
resource operands could substantiate the shared template, lazy resource indexes
could expose the template index, FIFO allocation previews could return the wrong
retained value, and expiration previews could ignore the once-per-frame gate.
All four reproduced on x64 and Win32 in [43367804486](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43367804486).
The lazy-index fixture uses the production template sentinel, -1; the earlier
fixture's default -2 accidentally wrapped to slot 0 and was corrected before
counting that reproduction. An additional accepted-resource case detects a
second GPU identity being created for a nested read in [43368276697](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43368276697).

`491ac5a2` replaces the separate preview lookup algorithm with detached pool
state that executes the ordinary lookup rules. It preserves the index map,
replacement cursor, expiration gate, slot types and values, including proxy
resolution. Live pools and the global element registries remain untouched
during metadata evaluation. Template configuration is copied into fresh lazy
slots without sharing the template's GPU object.

Resource operands can create provisional GPU resources in detached state.
Accepted ordinary reads adopt those resources only while their live source
still matches the snapshot, preserving resource identity and avoiding duplicate
creation; skipped copies release them. Initial buffer data is detached before
preview creation can resize it. Preview containers allocate only when metadata
is actually needed; existing source caches continue to bypass creation flags.
This is a correctness repair. Its cold-path copying cost is not separately
benchmarked, and earlier performance figures retain their original identities.

The suite now contains **42** native cases plus the three mutation-cleanup
failure cases. The existing expired-preview assertion retains its expected
default 3 and untouched live value 11 through the detached-preview API. Five
additional deliberate breaks cover the four warnings and accepted resource
identity, including a self-indexed source.

Verification runs for runtime/test snapshot `491ac5a2`, with the mutation harness correction in `54e6d4e5`:

- [Native x64/Win32 behavior, 42 compiling deliberate breaks, restored production and cleanup checks](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43369978270).
- [Ordinary/strict Release and strict Zip Release builds, formatting, scripts and shaders](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43369166788).
- [Sonar quality gate and changed-source clang-tidy against `origin/develop`](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43369170005).

The first detached-preview mutation run stopped because the template-only
replacement left the extracted `id` unused, producing strict compiler warning
C4189. That failed mutant build was not credited as detection. `54e6d4e5` removes
the index declaration with the replacement, preserving a compiling behavior
break; runtime and native-case source remain identical to `491ac5a2`.

The native workflow uploads the validated executable, per-case mutation hashes,
failure logs and restored-case logs. The earlier 35- and 37-case receipts remain
frozen historical snapshots. PR #43's latest checks and review record the final
integration endpoint; `develop`, `main` and installed-game validation remain
separate promotion steps.

### Hot resources, cached spatial data and static pins

Kilo's review of `f54e4cbb` identified three further lifetime/equivalence
warnings. Repeated adoption could clear a hot resource, runtime snapshots
omitted CPU-side cached data, and statically pinned operands could follow the
pool's current proxy during preview. All three reproduced on x64 and Win32 in
[43372079390](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43372079390),
while the prior 42 cases passed.

`9a43e4c5` consumes each preview once and keeps unchanged GPU objects and their
live caches in place. Runtime snapshots share ownership of CPU data and copy
the cache range and resource metadata; preview hashing can recompute local
memoized hashes. Adoption preserves an already-valid live cache. Static operands
clone their exact parsed resource, and adoption prefers that pinned snapshot
before looking up a physical pool owner. Ordinary STATIC proxy semantics are
preserved.

The suite now contains **45** native cases and matching compiling deliberate
breaks, plus the three cleanup failure cases. The spatial case uses a nonzero
CPU-cache range and the independently specified grid key `0x04002003` for unit
cells at (64,2,3), and checks that the same key remains available after the copy.
The other new cases protect hot GPU identity and a STATIC operand parsed before
its pool becomes a proxy.

Verification for runtime/test and mutation snapshot `9a43e4c5`:

- [45 native cases, deliberate breaks, restored production and cleanup checks](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43372700005).
- [Ordinary/strict Release and Zip Release, formatting, scripts and shaders](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43372702901).
- [Sonar and changed-source clang-tidy against `origin/develop`](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43372705996).

The earlier 35/37/42-case receipts retain their original sources and binaries.
These lifetime repairs have no new performance measurement. Installed-game smoke
and frame-time validation remain open.

### Shared aliases and one destination per copy

Kilo's review of `730ce6b5` added an alias warning and a valid test-coverage
suggestion. A STATIC pin and a dynamic alias of the same cold root slot could
create separate provisional GPUs. The [corrected control](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43376792457)
reproduces the extra creation on both architectures. Initial alias harness runs
stopped at parsing because subtraction lacked separating spaces; those were not
credited as runtime reproductions.

Pins now share the owning slot's snapshot when that owner is still the root and
the physical slot matches the pinned object. Pins whose pool has redirected
elsewhere continue to use their original object. The cached-range test now uses
different FIFO keys for the correct `0x04002003` and incorrectly offset
`0x06340002` data, with SRV support on only the correct entry. Its deliberate
break zeros the copied offset, so the test protects the preview range directly.

An additional expiration boundary checks both operand arrangements and verifies
that the GPU supports the SRV or UAV slot actually written. It assumes no
arithmetic operand evaluation order. The [valid failing run](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43378780324)
exposed a mismatch. Creation metadata is now captured before virtual expiration
can reset it. A [separate trace](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43381362157)
then showed preview index 0, first live index 0 and a later setter index 1.

`4bbd54ba` retains the resolved dynamic destination for the command's lifetime.
The setter reuses that resource, and copy creation uses its known flags.
Skipped `unless_null` copies still avoid live destination resolution. Full-pool
copies keep their existing per-slot handling. Trace instrumentation remains on
the separate control branch and is absent from the repaired source.

Verification for runtime/native and mutation snapshot `4bbd54ba`:

- [47 native cases, 48 compiling deliberate breaks, restored production and cleanup checks](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43381953009).
- [Ordinary/strict Release and Zip Release, formatting, scripts and shaders](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43381965227).
- [Sonar and changed-source clang-tidy against `origin/develop`](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43381995938).

Two independent mutations cover the expiration case: losing creation metadata
and resolving the destination again. Earlier receipts keep their original
source/binary identities. No performance percentage is attributed to these
correctness changes; installed-game and performance validation remain open.
