# Next release implementation plan

**Prepared:** 2026-11-12. **Status:** first wave native validated; follow-up profiler/parser/view candidate implemented on `upstream-improvements`, with PR #43 review repairs and 35 native cases validated; the game gate remains open.
**Next work:** review PR #43 and its analysis results, then installed-game smoke and frame-time validation before promotion. The execution log records accepted and deferred items;
[the implementation report](next-release-implementation.md) records the evidence.

## Objective and release boundaries

Improve reliability first, then reduce measured startup/rendering costs, then
consider larger architecture changes. Keep individual fixes reviewable and
preserve a releasable checkpoint before experimental optimization work.

| Checkpoint | Included work | Completion requirement |
|---|---|---|
| A: correctness candidate | Phases 1-3 | Behavior checks, strict native builds, analysis and EDHM smoke checks pass. This can ship independently. |
| B: performance candidate | Checkpoint A plus accepted Phase 4 changes | Each retained optimization has an isolated result and passes cumulative compatibility checks. |
| Later candidates | Selected Phase 5-8 projects | Each project clears its own entry/exit gate. These projects do not hold up A or B. |

Use the [intake audit](upstream-next-release.md) for decisions and the
[XXMI](upstream-xxmi-next-release.md) / [original](upstream-original-next-release.md)
appendices for exact patches, dependencies and source evidence. This plan owns
execution order and completion state; those documents own the detailed findings.

## Pinned starting points

| Purpose | Revision |
|---|---|
| Audited EDHM HEAD | `34d92f27f588bbc3f72320c0fa17b674638698c6` |
| Last local release | `v0.1.3`, `8d4272e5beb0203470afd5bdc3644e1fda4c510e`; same runtime source as audited HEAD |
| Actual selective XXMI port cutoff | `326f64cf9ab565aa7cd3d50e002270994b8d16c6` |
| XXMI reference for candidate selection | `065dc746c21c71ef48bbc93cb6b90b3dcdf766e6`, v1.2.3 |
| Original 3Dmigoto reference | `8f329bd94fecc9bbcb9211ffd42a95dd7fe6b43e` |
| Agreed historical performance baseline | XXMI v0.9.8, `f8e7118a1220e43546e546efa31892b8304358df` |

- [x] Upstream refs audited and `xxmi-base` fast-forwarded locally and on GitHub.
- [x] Existing equivalents and incompatible upstream behavior documented.
- [x] Implementation baseline and build access established.
- [x] Phase 1 test behaviors confirmed by the instruction to execute this plan; additional cases are being confirmed as phases begin.

The historical benchmark and candidate-selection reference serve different
purposes. For each new optimization, compare the same current EDHM base with
and without that change; retain the historical v0.9.8 comparison separately.

## Rules for executing this plan

1. Use an isolated feature checkout from the verified integration base. Keep the
   current dirty checkout, `addons/`, audit artifacts and installed game intact.
   Use descriptive branch names such as `upstream-parser-fixes`, without a
   `codex/` prefix. Keep `xxmi-base` as reference code.
2. Read the relevant audit entry and inspect the current function before each
   port. Reassess if HEAD or upstream pins have changed. Preserve local fixes
   already equivalent to upstream; take only the missing behavior.
3. Keep each behavior in a separate patch/commit. Port the final needed semantics
   and prerequisites, preserving local formatting, ownership and APIs. A clean
   patch application is only a text check.
4. Before adding tests, present/refine the observable behavior cases and obtain
   the confirmation required by the user's testing guidelines. Reuse any
   confirmation already given. Check existing coverage first. Verify each new
   test by deliberately breaking its protected behavior, observe failure, and
   restore production code before continuing.
   For bug fixes, prefer a failing behavior test before the production change
   when the available harness permits it.
5. Keep tests hermetic. Use literal expected outcomes and mock only real external
   boundaries. Use native D3D validation where COM/pipeline behavior is the
   contract; parser/helper tests alone cannot prove rendering correctness.
6. Run the checks appropriate to each patch. Complete the phase gate before
   marking it done or building a dependent phase on top. Record unavailable
   checks as pending; report unrelated baseline findings separately.
7. Preserve real input-layout pointers and owned side-cars, hash-first EDHM
   matching, early includes, Wine/DXVK chaining, quiet logging, safe snapshots,
   bounded hash probes and correct multi-page invalidation.

## Phase 0: prepare an executable baseline

- [x] P0.1 Record working-tree status, integration SHA and upstream pins. Verify
  `origin/develop` against the audited runtime before choosing it as the base.
  Make the plan/audits available from the feature checkout without absorbing
  unrelated files from the original dirty checkout.
- [x] P0.2 Establish an MSVC v143/Windows SDK execution path through an available
  Windows build host or existing CI. Follow [building](building.md) and
  [code quality](code-quality.md). Linux/MinGW/Wine checks are supplementary.
- [x] P0.3 Capture an unchanged strict Release baseline and existing focused-test
  result. Record baseline analyzer/build failures before editing production.
- [x] P0.4 Confirm the Phase 1 test behavior table below. Locate existing coverage
  and choose the smallest suitable native test harness; add a test seam only
  when needed to exercise observable behavior.
- [x] P0.5 Record baseline DLL/config/shader hashes for later smoke/profiling
  comparisons. The installed DLL's version string alone does not identify its
  source. Keep original files recoverable and distinguish current diagnostic
  settings from the normal-play configuration.

**Exit:** isolated source, recorded baseline, usable native validation path and
confirmed initial test behaviors. If native validation is unavailable, finish
independent preparation and leave the execution/validation gate open.

## Phase 1: small crash, settings and syntax fixes

Prefer this order: prevent crashes and lost pending saves, restore valid syntax,
then repair small metadata/parser omissions. Use separate patches.

| Item | Change and source | Observable behavior to protect |
|---|---|---|
| P1.1 | Empty-token guard in `transform_operators_visit`, XXMI `c4e3a449` | `()` and a nested empty group report a configuration syntax error; valid grouping still evaluates correctly. |
| P1.2 | Narrow failed-save repair in `SavePersistentSettings`; adapt the final persistence behavior described in the XXMI audit | A failed file open preserves pending dirty settings for retry and restores the previous locale. A later successful save writes the pending value. Preserve existing save scheduling and sticky dirty updates. |
| P1.3 | Leading-dot numeric tokenization, `a9d7c89b` | `.125` evaluates to literal `0.125`; negative values and subtraction remain correct; malformed numeric input is rejected. |
| P1.4 | Pool proxy reset routing, `ad264db8` | `PoolFoo = null` reaches proxy-reset behavior and does not reset unrelated pools or variables. |
| P1.5 | Constant-buffer stride cleanup, `6b2d803f` | Recreating a structured source as a constant buffer clears the structured flag and stale element stride while keeping valid contents/size. |

P1.2 is deliberately smaller than the later unknown-settings lifecycle redesign.
The source audit found dirty state cleared before opening the file and an early
return before locale restoration. Validate that behavior first; keep partial
write/atomic-save policy explicit for Phase 5.

**Exit:** each confirmed case passes, each added test has demonstrated failure
under a deliberate behavior break, changed code is formatted, strict native
builds pass, and no new correctness/security findings remain.

## Phase 2: resource references and pool metadata

- [x] P2.1 Port `038af009` across `CommandList.cpp/.h` and `IniHandler.cpp`.
  Collect reference edges, clear them before resource teardown and resolve flags
  after all command lists are parsed. Verify equivalent resource/view creation
  for different section orders, a multi-hop chain and reload; include incompatible
  flag combinations and cycle termination.
- [x] P2.2 Port `7d2c4705` so bind-flag queries use pool-template metadata without
  lazily creating/touching an element. Verify null-source `unless_null`, element
  lifetime/expiration and explicit static-element behavior.
- [x] P2.3 Port `f9d665e2` only after confirming pool-identifier compatibility
  cases: namespaced/hyphenated/Unicode pool variables plus ordinary-variable
  fallback. Keep local UTF-8 and subtraction parsing intact.

Confirm these behavior cases before adding tests. **Exit:** order-independent
reference behavior, non-mutating metadata queries, compatible identifier parsing
and the shared validation gate pass, including reload lifetime checks.

## Phase 3: structured readback and store ownership

- [x] P3.1 Define and confirm `store` behavior for null/non-buffer sources,
  negative/nonfinite/out-of-range indices, exact-fit reads and readback failure.
  Preserve existing successful flat-float indexing; avoid inventing a new error
  result without documenting its compatibility effect.
- [x] P3.2 Adapt `24e2cff7` to copy complete structure elements using wide checked
  byte arithmetic. Validate source type before buffer APIs and index validity
  before conversion. Read the desired float with `memcpy`; retain staging reuse.
- [x] P3.3 Balance both source-view and resource references on all exits. Verify
  repeated SRV/custom-view reads and error paths through the real COM ownership
  boundary. Generalized CPU data caching is outside this patch.
- [x] P3.4 Validate typed/structured reads and final-value boundaries in a native
  D3D fixture; use the debug layer for correctness checks and Release without it
  for any timing. Confirm no unintended extra readbacks on existing flat reads.

**Exit:** supported reads return hand-computed values, unsupported inputs follow
the agreed policy, lifetime validation passes, and native builds/analysis pass.

## Checkpoint A: first correctness release candidate

- [x] Review the Phase 1-3 patch stack and its source provenance.
- [x] Run cumulative x64/Win32 strict Release builds and the focused behavior
  suite; run the existing shader-hunting tests and required shader validation.
- [ ] Verify EDHM startup/HUD colors, location load, docking, F11 reload, theme
  auto-refresh, `ClearState`/layout behavior where exercised, and shutdown.
  Include Linux/Proton; record Windows integration checks separately.
- [x] Document included changes and any explicitly deferred item. Prepare a
  reviewable candidate without waiting for later optimization projects.

**Exit:** a validated correctness stack is available independently of Phases 4-8.
Release publication/version changes follow the separate release procedure and
the authorized release scope; this plan does not publish a release by itself.

## Phase 4: limited, independently measurable performance work

Start from Checkpoint A. For every item, retain one-change A/B source and binary
hashes, raw observations and a cumulative-stack result. Drop a proposed
optimization if it changes required behavior; defer claims below measurement
noise. Correctness fixes remain justified independently of timing.

| Item | Candidate | Required result |
|---|---|---|
| P4.1 | Constants compaction, `7ea5e930` | Stable execution/declaration ordering and errors; isolated startup/reload time on the actual EDHM tree. |
| P4.2 | Static-condition memoization, `d8937803` + relevant `0271e6f7` cleanup | State resets on a new configuration; static/dynamic conditions retain outcomes; optimizer time improves where active. |
| P4.3 | Current INI section reuse, `06962f17` + `8d156caa` | Duplicate/recursive section semantics unchanged; fewer lookups and a measured parse/reload result. |
| P4.4 | Buffer contamination early exit, `abaed4e4` | Balanced profiler/locking exits; tracking-on/off and buffer/texture workloads show total cost, including added `GetType`. |
| P4.5 | Compatible source-view reuse, `ace1f29f` | Correct resource/view interface and COM lifetime; fewer actual view creations without changed bindings. |
| P4.6 | Optional local duplicate-disassembly cleanup from the inherited audit | Patterned ShaderRegex misses produce identical replacements with one disassembly call; cold first-use timing kept separate from warm play. |

Use existing profiling modes in separate diagnostic runs because they add
overhead. Record load/reload milliseconds separately from frame-time tails.
Keep `dump_usage`, debug logging, hunting and region tracking activation explicit.
Use the measurement protocol in [the performance audit](performance-audit.md#protocol-for-finishing-per-change-in-game-attribution).

**Checkpoint B:** retain only independently validated changes and repeat the
Checkpoint A cumulative gate. Smaller parser dispatch/normalization work
(`1ff00d94`, `87fa9479`, `ec9e5713`, `5c77f4b0`) follows only if remaining parse
cost justifies it; preserve precedence, Unicode namespaces and identifier rules.

## Phase 5: larger settings and existing-region correctness projects

Keep these as independent branches/patch series from a validated checkpoint.

- [x] P5.1 Consolidate the final unknown-persistent-settings lifecycle using the
  [persistence dependency inventory](upstream-xxmi-next-release.md#3-persistent-settings-must-move-as-one-behavior-change).
  Specify retention, grace reload, removal of `persist`, explicit wipe and failed
  writes before implementation. Preserve Phase 1's failed-open repair and decide
  how write/close failures and partial writes retain recoverable state. Exercise
  the behavior against an isolated file through reload/autosave sequences;
  preserve local logging and theme reload coordination.
- [x] P5.2 Repair already exposed `->Region(offset,size)` operations with
  `532a37c5` + `c226158e`. Verify byte count versus end offset, CB view window
  versus backing extent, exact bounds and overflow. Dynamic slot-range syntax
  is not required for these repairs.
- [ ] P5.3 If an intended mod uses stream-output targets, port `1d9403c0` +
  `75e52ba6` with the needed reference-array ownership from `7bea1274`. Verify
  target offsets and preservation of other slots' append positions.

**Exit per project:** agreed data/region behavior, failure cases and native
integration checks pass. Release valuable completed projects independently.

## Phase 6: EDHM-specific TextureOverride candidate cache

**Entry:** profiling demonstrates material repeated matching cost; Phase 5
repairs are included wherever the cache path depends on them. Design from
`fb2b5b0d` / `034e2875`, preserving local semantics.

- [ ] Specify the cached versus live predicates and synchronization/lifetimes.
  Keep hash-first matching after draw filters, explicit dual matching and
  untracked-resource fallback.
- [ ] Cover resolution-dependent predicates with a generation/key or live
  evaluation. Invalidate on effective resolution, configuration and hash changes
  and before override/resource lifetime ends.
- [ ] Demonstrate identical ordered match sets/filter indices with cache off/on
  across reload, resize, resource churn, hash updates, priorities and region paths.
- [ ] Measure hit/rebuild cost, memory, contention and total frame outcomes.

**Exit:** semantic equivalence and invalidation/lifetime gates pass, and real
workload benefit supports retaining the cache. A fast stale cache fails the gate.

## Phase 7: conservative SRV batching, then conditional batching

**Entry:** an observed command workload contains useful eligible runs. Keep this
project independent of Phase 6; matching caching is not a batching prerequisite.

- [ ] P7.1 Start with a restricted fixed-slot subset based on `9f04a753` and
  needed refinements. Define eligible sources/options and preserve global INI
  execution order across slot gaps and duplicate slots.
- [ ] P7.2 Preserve prior bindings when resource creation fails; distinguish
  failure from intentional null assignment and `unless_null`. Exclude
  `set_viewport` and other uncertain side effects until equivalence is proven.
- [ ] P7.3 Compare batched/unbatched binding/output behavior for gaps, duplicates,
  failure, nulls, stages and pre/post phases. Measure eligible-run frequency,
  getter/setter reduction and total CPU/frame cost.
- [ ] P7.4 Add conditional batching only after P7.1-3 pass, using the corrected
  PR68 dependency list in the XXMI audit. Verify nonzero slots, missing else,
  nested branches and pipeline-reading conditions.

**Exit per stage:** equivalent outcomes and useful measured benefit. Keep the
first stable subset releasable without conditional/dynamic/range expansion.

## Phase 8: workload-driven features and optional tooling

| Project | Prerequisite / decision |
|---|---|
| Cache-first `store`, `0c7905e2` + `1fba754a` | Depends on Phase 3 and complete write/copy/clear invalidation. Require real readback stalls; measure upload work and RAM as well as hits. Independent of conditional batching. |
| Broader dynamic/range syntax or bytecode reflection | An actual mod/authoring need and a separate compatibility design. Include associated bounds, lifetime and concurrent-reader fixes. |
| Original raw-buffer/Texture1DArray decompiler, `f63fe792` | Separate shader-authoring project with successful disassemble/decompile/compile behavior checks for components and register classes. Keep local parsing bounds; defer incomplete support. |
| Original header hygiene | `32d6670d` / `91986f01` are optional small build-only cleanups, useful alongside affected code but never release blockers. |
| Disabled-hunting fast path | First measure the lock cost; require a safe reload/concurrency design before removing synchronized state access. |
| Namespace, logging, timing or input-policy changes | Require an intentional behavior requirement; preserve EDHM defaults during unrelated ports. |

## Shared verification and delivery gate

For every completed phase:

- [x] Changed code passes the repository formatter and relevant static analysis;
  strict MSVC Release builds pass for x64 and Win32. Use the maintained commands
  in [code-quality.md](code-quality.md), including Build Wrapper/Sonar and
  clang-tidy where required. Bootstrap pinned tools once, not per patch.
- [x] Focused behavior checks pass; new tests have recorded deliberate-break
  evidence. Existing tests retain their expectations.
- [x] Record source/patch SHA, upstream provenance, binary SHA, validation output,
  residual limitations and the next action. Keep large logs under ignored
  `.quality/` paths and preserve summarized results with the patch.
- [x] Run cumulative native compatibility and package/Zip Release checks for
  both architectures.
- [ ] Complete installed-game compatibility checks and report platform gaps
  before promotion.

Track each item as `pending`, `in progress`, `validated`, `deferred` or `blocked`.
A deferred item has a reason and re-entry condition. An unvalidated item remains
open; it does not silently become part of the completed release scope. If a
behavior regression occurs, isolate/revert that candidate on its feature branch
and keep the previous validated checkpoint usable.

## Execution log

| Item | Status | Patch / evidence | Next action |
|---|---|---|---|
| Reference update and audit | Complete | `upstream-next-release.md` and its data JSON | Use pinned sources for the first implementation wave. |
| Phase 0 | Validated | Clean sibling worktree from `34d92f27`; [strict baseline CI](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43296386477) passed x64/Win32, shaders, formatting and scripts. Existing hunting tests passed under MinGW/Wine. Installed inputs hashed separately. | Preserve original checkout and installation. |
| P1.1-P1.5 | Native validated | Separate patches; original parser/save defects detected by baseline cases. Native x64/Win32 checks and deliberate-break verification pass. | EDHM game smoke/frame-time gate. |
| Phase 2 | Native validated | Reference chains work in both orders for SRVs and CBs; incompatible cycles terminate without changing rejected flags. Pool expiration/static metadata and identifier cases pass. | EDHM game smoke/frame-time gate. |
| Phase 3 | Native validated | Typed/structured boundaries, nonfinite/negative indices, non-buffer sources, allocation failures and custom/pipeline view lifetime checks pass. Failed reads retain the previous value. | EDHM game smoke/frame-time gate; real device-loss Map failure remains an integration case. |
| P4.1 | Evaluated / deferred | Corrected single-function load→optimize comparison: 74.485 → 74.162 ms load, total process change interval crosses zero. Earlier parse-only figure is superseded. Compaction removed in `b14272fa`; ordering checks retained. | Re-evaluate only for a declaration-heavy actual workload. |
| P4.2 | Evaluated / deferred | Static If memoization reduced optimizer median by only 0.011 ms; useful total benefit not established. | Require material optimizer cost in a real configuration. |
| P4.3 | Measured / retained | Section reuse: 73.971 → 71.625 ms median load; 74.792 → 72.484 ms load+optimize. Eight processes per variant; `dc015894`. Raw data and hashes in `benchmarks/startup-evaluations.*`. | Cumulative/native checks pass; game gate remains. |
| P5.1 | Native validated | Confirmed policy implemented with full-set comparison, one-load grace, optional retention, nonpersistent filtering, atomic writes and failed-reload preflight. | EDHM theme/reload smoke. |
| P5.2 | Native validated | Region byte counts, backing extents, views, CB copy/ref binding and alignment checks pass native behavior cases. | EDHM game smoke. |
| Shared native/quality/package gate | Validated | [Release/Zip](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43315053539), [20 cases + 20 detected mutations](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43315064363), [Sonar/clang-tidy](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43316212003); zero unresolved Sonar issues. Cumulative configuration result: 74.044 → 71.806 ms. | Installed-game gate before promotion. |
| P4.4-P4.6, P5.3, Phases 6-8 | Assessed / deferred | Workload/profile requirements assessed in the implementation report. | Follow each explicit re-entry condition; no unmeasured rendering cache or batching enabled. |

The detailed completion evidence, deviations needed for correctness, measurement
scope and remaining gates are recorded in [the implementation report](next-release-implementation.md).
Independent correctness and startup evaluations proceeded while the game gate
remained open; no checkpoint was promoted on that basis. Native validation does
not close the installed-game smoke or FPS-attribution gate.

## Follow-up profiler, parser and view work

Starting point: `afe3372b`; selected candidate: `83b46492`. The user requested
this next wave after reviewing the remaining opportunities. Measurements and
architecture details are in the final section of the implementation report.

- [x] Add summary-mode resource/fuzzy matching and view API/reuse diagnostics.
- [x] Evaluate once-per-window profiler registration with profiling-off controls
  and same-runner native A/B processes; retain the generation-based path.
- [x] Evaluate operator-category masks, direct traversal, their combination and
  namespace-key construction independently on the EDHM snapshot. Leave all
  three parser changes out after isolated and final combined-build controls.
- [x] Implement conservative source-view reuse with resource/interface/descriptor
  checks, explicit-region and override protection, owned queried interfaces and
  failed-view binding preservation. Evaluate stable and changing resources.
- [x] Run existing native cases and approved mutation checks on the combined
  candidate; strict Release/Zip Release and shader checks pass.
- [x] Receive recommended-case confirmation for both prompts.
- [x] Add nine follow-up cases alongside the existing 20; repair the warm-view
  metadata defect exposed by the new checks. Native x64/Win32 cases pass.
- [x] Finish the expanded 29-case mutation run and final analysis/measurement
  gates on `follow-up-regression-checks`, then collect the validated work on
  `upstream-improvements`. All 29 compiling mutants were detected; restored
  production, strict builds and Sonar/clang-tidy passed. Cache-validation CPU
  costs and the retained creation-heavy benefit are recorded separately.
- [x] Finish combined static analysis and record the cumulative configuration
  result: 72.032 → 72.090 ms load, with no additional startup gain established.
  Sonar/clang-tidy passed for `83b46492`; linked evidence is in the report.
- [ ] Capture a repeatable Elite scene, using diagnostics separately from
  profiling-off frame-time outcomes; leave caching/batching gates closed until
  the workload justifies them.

The user approved both recommended case lists before these new cases were
written and allowed separate test/benchmark branches. The implementation report
records pre-fix failures, the cache repair and the resulting native checks.


## PR #43 review corrections

Eight of Kilo's nine findings reproduced on the isolated `pr43-review-fixes`
branch; the formatting claim passes with the pinned tool. The approved suite
now has 35 native cases and three mutation-runner cleanup cases. Build-language,
CB region, clear-view, pool metadata, scalar precedence, `this` cache, zero-frame
profiling and mutation-restoration repairs are described with red/green evidence
in [the implementation report](next-release-implementation.md#pr-43-review-follow-up).
The tested repair checkpoint is collected on `upstream-improvements`; the linked
checks record final native, build and analysis results. The PR continues to target `develop`; game validation
remains a separate gate before release promotion.
