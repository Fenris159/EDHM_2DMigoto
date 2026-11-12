# Next release implementation and validation report

**Date:** 2026-11-12. **Branch:** `upstream-improvements`.
**Integration base:** `34d92f27f588bbc3f72320c0fa17b674638698c6`.
**Runtime candidate:** `b14272fa74a3221092ccd6279e15a08463f864d6`.

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

The code is prepared for review on a separate branch. The original dirty
checkout and installed game files were preserved. Strict builds, native regressions, mutation sensitivity and static analysis pass.
Game smoke and frame-time validation remain open.

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
| P4.5 broader compatible-view reuse | Defer. Explicit-region view repair is included; general reuse needs resource/interface/descriptor/lifetime proof. | Actual view-creation counts and timing, with identical bindings and owned lifetimes. |
| P4.6 duplicate ShaderRegex disassembly | Defer. Installed lexical scan found no active ShaderRegex declarations. | A real patterned-miss authoring workload, identical replacements and cold first-use measurements. |
| Smaller parser dispatch/normalization | Defer. Section reuse is measured; broad grammar changes have a larger compatibility surface. | Remaining parser hotspot attribution and namespaced/Unicode/precedence equivalence. |
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
