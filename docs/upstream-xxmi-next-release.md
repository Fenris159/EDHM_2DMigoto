# XXMI changes proposed for the next EDHM release

Audit date: 2026-11-12. This is an implementation-selection audit, not a port.
Production code was not changed and no game benchmark was run for this document.

## Decision brief

Prepare a small correctness release first: reject empty parenthesized expressions,
repair structured-buffer `store` with balanced view ownership and source-type
validation, accept pool proxy reset, avoid pool-element creation during metadata
queries, propagate reference-chain bind flags, and clear structured stride when
converting to a constant buffer. Consolidate the persistent-settings lifecycle
as a separate reviewed change. These address observable failures and retention
without needing an FPS claim.

For performance, start with the buffer contamination early exit, then independent
load/reload parser changes. Consider compatible source-view reuse next. The
TextureOverride candidate cache and SRV batching need manual adaptation and
additional correctness work before inclusion; inspection found issues beyond
the already documented hash-first and conditional-slot compatibility risks.
Keep generalized CPU buffer caching optional until a workload demonstrates
readback pressure. No measured per-change Windows/Elite speedups establish a
different ranking.

## 1. Exact comparison and limits

| Role | Commit |
|---|---|
| Audited EDHM runtime | `34d92f27f588bbc3f72320c0fa17b674638698c6` |
| Latest local stable `v0.1.3`, peeled | `8d4272e5beb0203470afd5bdc3644e1fda4c510e` |
| Historical XXMI tag `v0.9.8` | `f8e7118a1220e43546e546efa31892b8304358df` |
| Recorded selectively ported milestone | `326f64cf9ab565aa7cd3d50e002270994b8d16c6` |
| Latest fetched XXMI `master` / `v1.2.3` | `065dc746c21c71ef48bbc93cb6b90b3dcdf766e6` |

The recorded port reaches beyond the `v0.9.8` tag. Use the `326f64cf..065dc746`
range to review work absent from that milestone: 236 commits, including 178
non-merge commits; 68 changed files, 10,451 inserted and 1,257 deleted lines.
These are change-volume facts, not missing-feature counts or performance data.
Some commits in the range come from older side branches. A calendar filter
after local release day would miss applicable, older changes not yet ported.
Conversely, cherry-picks/manual equivalents can already be present without
shared ancestry. Each proposal below checks current code.

Local `v0.1.3..HEAD` changes only the release workflow's strict-build flag, so
the runtime findings apply to both. The release-baseline evidence is in
[audit-upstream-baselines.md](audit-upstream-baselines.md), and the recorded port
is in [patches/0002-xxmi-since-2026-07-17.md](../patches/0002-xxmi-since-2026-07-17.md).

Read-only patch checks are recorded in `.quality/upstream-next-release/patch-applicability.csv`.
Only `ad264db8`, `6b2d803f`, and `4cd308a5` among 32 selected commits apply
unchanged under the checks performed; the others require manual reconciliation.
This is patch-text applicability, not a merge simulation, build, or correctness result.

This audit used local/fetched immutable source, introducing and follow-up patches,
and final `v1.2.3` source. Findings called out as hazards are static source
deductions; they have not been reproduced in a D3D runtime here. Proposed
verification cases are requirements for implementation, not tests written or
executed during this documentation task.

## 2. Correctness candidates for the first release

| Priority / change | Current EDHM evidence and proposed action | Exact upstream source / dependencies |
|---|---|---|
| C01: Empty expression guard | `transform_operators_visit` checks `tree` but proceeds to `rbegin()+1`, `rend()-1` or `begin()+1` for an empty grouped expression. Port the small explicit empty-token rejection before iterator arithmetic. Keep local exception/report handling. | [c4e3a449](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/c4e3a4499d05268bfef3896ff4eeee8ed18a34be). The subsequent [661dde0b](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/661dde0be647166b60b2ee01a9593091f290408b) parenthesis rewrite is optional cleanup, not required for the guard. |
| C02: Structured `store` and local lifetime repair | Current `StoreCommand::run` always stages four bytes using unchecked `offset*sizeof(float)` arithmetic, regardless of structure stride; it does not validate source buffer type. Port whole-structure aligned copies, 64-bit byte math, bounds and exact-fit handling. Add the local view-ownership and type checks described below; upstream alone does not supply them. | [24e2cff7](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/24e2cff7b2bbfde31a8882c06d178fc008152997), refined GPU-read helper in [1fba754a](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/1fba754a6bf50e5fbeb01e3087353b296595661e). The latter also adds cache/pool features that can be omitted from this first correctness port. Use its `memcpy` extraction rather than an unaligned `float*` dereference. |
| C03: Pool proxy reset syntax | Parser dispatch accepts pool source for a pool destination but rejects `PoolFoo = null`, even though `PoolCopyOperation::run` already implements EMPTY-source reset. Add EMPTY to that dispatch condition; retain the distinction between a pool proxy reset and a full-range reset. | [ad264db8](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/ad264db86ee7d4870215edce95287614713f2a41). Independent of the newer slot-range architecture. |
| C04: Non-mutating pool bind-flag metadata | Current `ResourceCopyTarget::BindFlags` calls `GetCustomResource(state)`, which can resolve/create/touch a dynamic pool element before `unless_null` can cancel a copy. Resolve the template without runtime state for metadata, preserving direct static-element behavior. Keep null safety appropriate to local resolution. | [7d2c4705](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/7d2c47051f1cceb6abe4294e391bcc526d678e50). Check local pool-template flags/propagation; pair logically with C05. |
| C05: Reference-chain flag propagation | Current parse-time resource copies propagate one hop. A file resource referenced through aliases can miss required SRV flags depending on section parse order and bind a null view. Record ref edges, reach a fixed point after all command lists parse, and clear edges before config resources are discarded/reloaded. | [038af009](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/038af0099ed5be1c33fe1e8a7485fffd03ed704a), touching CommandList.cpp/.h and IniHandler.cpp. This adds load-time work for correct creation, not a proven runtime speedup. |
| C06: Structured-to-CB descriptor | `RecreateCompatibleBuffer` clears the structured flag but leaves source `StructureByteStride`; metadata getters can report a structure stride for a nonstructured constant buffer. Clear it with the flag. | [6b2d803f](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/6b2d803f50892ee87685e80ce4f561bdeb12f173). Small independent manual port. Preserve EDHM range/overflow protections. |
| C07: Leading-dot numeric literals | Tokenization attempts numeric parsing only for digit-leading text. Accept `.123` as a numeric candidate, preserving subtraction-first tokenization and the existing numeric parser's range checks. | [a9d7c89b](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/a9d7c89b1ba957ac8a65a8bbf72acefa86eec544). Independent small compatibility fix. |
| C08: Pool-variable identifier grammar | Current tokenizer first limits `$` variables to regular variable identifiers; hyphen/Unicode/namespace pool names can fail before the looser target parser. Try a `$pool` candidate through the resource-target grammar before regular-variable fallback. | [f9d665e2](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/f9d665e220d8551015b88b5bb22976929e509bdf). Preserve fallback for ordinary names such as `$pool_test` and local UTF-8/hyphen behavior. |
| C09: Persistent-settings lifecycle | Current unknown settings schedule a rewrite and `SavePersistentSettings` serializes only currently recognized variables. Autosave can remove temporarily absent settings sooner than the reload warning implies. Separate unknown recognition/retention/cleanup from ordinary dirty saves, as a consolidated manual port. See section 3. | Final lifecycle [015d3394](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/015d33945e12b2d355c2735129083cdcd5a5c1b9), persistence eligibility [c0675d4b](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/c0675d4beb7f737eac0347b6bd400d26f74c3dd9), append support [fa3c42cd](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/fa3c42cd5ba5d9214798aff80aa7c089e0aa89a9), dirty follow-up [60f2cd44](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/60f2cd44b547bac50972d8c9039017f3dbcf7d92). Do not take the original autosave patch alone. |

Current-source anchors: [StoreCommand](https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/CommandList.cpp#L1756),
[empty-tree transform](https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/CommandList.cpp#L5011),
[pool metadata](https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/CommandList.cpp#L9537),
[persistent save](https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/IniHandler.cpp#L5505).

### Store defects not closed by latest upstream

Both local `StoreCommand::run` and final XXMI obtain `src_view` using
`src.GetResource(...,&src_view,...)` but never release that view. The source
resolver returns an owned view reference: pipeline SRV lookup obtains one from
`XXGetShaderResources`, and a custom resource explicitly AddRefs its stored
view. Repeated `store` from those sources therefore retains view references
even though the resource reference is released. Balance both objects on every
exit, preferably through scoped ownership. This is established from the local
ownership contract, rather than a speculative performance claim.
[Local resolver](https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/CommandList.cpp#L8988),
[latest StoreCommand](https://github.com/SpectrumQT/XXMI-Libs-Package/blob/065dc746c21c71ef48bbc93cb6b90b3dcdf766e6/DirectX11/CommandList.cpp#L1758).

Latest upstream also casts an arbitrary resolved `ID3D11Resource*` to
`ID3D11Buffer*` before `GetDesc`, without checking its dimension. The generic
target parser can resolve texture-backed views. Reject unsupported source types
or use a checked interface query before buffer APIs. Define behavior for invalid,
negative, nonfinite and excessive offset expressions before converting to UINT;
the upstream structure-copy patch does not by itself establish that policy.
Verify flat values, final valid value, structured elements, a value spanning a
structure boundary, out-of-range indices, null source, non-buffer source,
readback failure and repeated SRV-backed invocation. Do not turn on generalized
buffer caching merely to repair `store` correctness.

### Existing equivalents to skip

| Upstream change | Current EDHM status |
|---|---|
| [7ff1fe7b](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/7ff1fe7b1f7f1914abad0bd2644bde3cae3dfc68): error blob release when logging off | Already implemented by local [5c678bc0](https://github.com/Fenris159/EDHM_2DMigoto/commit/5c678bc068801d124c33a497d953ef92c5bb9aa3). `ReplaceHLSLShader` releases independently of `LogFile`, initializes outputs and guards error buffer/size. Keep the stronger local implementation. |
| [57a1b6a2](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/57a1b6a21d06ccfa8b24433b296eed83963ed8a9): include Y in spatial maximum | Already implemented with nested `std::max`, wider arithmetic, overflow checking and exact-fit support by local [24ebce13](https://github.com/Fenris159/EDHM_2DMigoto/commit/24ebce13fe1b16fc577c0ab8f4e7886c6ccab108). Do not replace it with the weaker original UINT formula. |
| [60f2cd44](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/60f2cd44b547bac50972d8c9039017f3dbcf7d92): unchanged persistent assignments must not cancel prior dirty state | Local assignments already use `|=`. The regression was introduced upstream during boolean dirty-state conversion [13f88a7a](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/13f88a7ac6195d77d59d884558ac3231cc908a99); retain the final sticky behavior if porting the lifecycle. |
| [a24d95f2](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/a24d95f23bfc92a7f9c08956f563e2471152f1c7): avoid questionable process-detach cleanup | Local DllMain already skips nontrivial teardown on process termination and retains explicit-unload cleanup, with its own TLS/once/Wine protections. Compare semantics instead of overwriting it. |
| `bf929475`, `fdab51e2`, `4e7988e7`, `98d0d98d`, `326f64cf` | Recorded port includes these recoverable-failure/tokenization/FIFO/spatial-pool fixes. Their presence in a broader v0.9.8 ancestry range does not make them missing work. |

## 3. Persistent settings must move as one behavior change

Do not cherry-pick [7d60f507](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/7d60f507eb4302f3a274b62c3fe8cd5c8629cc49)
alone. It rereads existing text during saves and was revised repeatedly.
The relevant evolution includes `dd1a1a57`, `6d6dfb60`, `2d6af722`, `c05b4cae`,
`79fd6eda`, `efb0c050`, `8c439438`, `13f88a7a`, `c0675d4b`, `fa3c42cd`,
`015d3394`, and `60f2cd44`. Port the final responsibilities into local logging
and reload coordination, rather than importing all intermediate states.

The final model recognizes unknown values, stores them separately, hashes the
ordered name/value set, grants one reload grace period by default, and offers
`clear_unknown_settings=0` for retention. Cleanup compares values as well as
count, so replacing one unknown with another does not prematurely remove it.
Settings that lost `persist` or `pool_persist_variables` must not regain their
old value from `d3dx_user.ini`. Ordinary autosave writes known and retained
unknown settings consistently; clear/wipe behavior remains explicit. The append
path requires the changed `wfopen_ensuring_access` contract. Latest save logic
retains dirty state when file opening fails and restores locale on that failure;
local code clears dirty first and exits before locale restoration on failure.
[Final responsibilities](https://github.com/SpectrumQT/XXMI-Libs-Package/blob/065dc746c21c71ef48bbc93cb6b90b3dcdf766e6/DirectX11/IniHandler.cpp#L4998).

Keep local int dirty-bit semantics until the unknown-state path is fully
separated. If changing to bool, update every setter, constants-initialization
reset, override handling and autosave/reload call site together, including the
sticky-dirty follow-up. Preserve EDHM signal-file reload behavior and quiet log
policy; the persistence series overlaps upstream's unrelated `log_level`
migration. Back up user settings for release validation, use an isolated file,
and verify startup, autosave, theme reload, repeated reload, disabled includes,
reenabling includes, removal of persistence flags, unknown-value change with
unchanged count, cleanup disabled, explicit wipe and failed writes. Final source
is not a guarantee of atomic persistence: save/append failures and partial writes
still need a deliberate local recovery policy.

## 4. Performance candidates after correctness

| Order | Candidate and exact upstream commits | Integration decision and metric |
|---|---|---|
| P01 | [abaed4e4](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/abaed4e420f5aec115a169740db74876874ed629): buffer exit before contamination locks | Small manual port into `MarkResourceHashContaminated`, retaining balanced profiling exits. Buffers bypass global locking/resource lookup because contamination tracking supports 2D/3D textures. Extra `GetType` may cost texture-only workloads. Measure tracking CPU µs/frame and call/type counts; verify hunting and texture-update behavior. |
| P02 | [7ea5e930](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/7ea5e9306c51234301781d226766c01540270c32): constants vector compaction; [d8937803](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/d89378035550033fc2949d39813ac852de1af6b6) plus [0271e6f7](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/0271e6f79b78c449d6b66d1782091d34e6b24415): memoized static conditions | Port separately. Replace repeated vector erasure with one compaction and tail erase; cache static-evaluation results for repeated optimizer visits. Preserve declaration errors/order and lifetime reset on new config. Report milliseconds for startup/reload/optimization, not gameplay FPS. |
| P03 | [06962f17](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/06962f170a36eeaf9bfa5cd9d7ce4b748fbf56cd) + [8d156caa](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/8d156caad87180bf038f0892451a6a6a91fcf264): pass INI section object; [1ff00d94](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/1ff00d94096fffef76122bde8c0c88eb1dc1253b) + [87fa9479](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/87fa9479cfc8e1f5c1ae9dcdcd3cb2a4d3a336d8): operator-category skipping/direct traversal | Independent parse-time reductions after C01/C07/C08. Retain local UTF-8/path handling, duplicate-section behavior and numeric guards. Verify operator precedence/associativity and nested forms before timing an actual configuration corpus. |
| P04 | [ec9e5713](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/ec9e57136d66738e6fb9ceb7501483be4e3c119d) + [5c77f4b0](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/5c77f4b0907906751626bb28e831edfeb22851b6), then [e624dcf1](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/e624dcf1fa1b66a61618d7f6164303d604456372) / [8d4d2cb3](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/8d4d2cb3cab28fccc151b2e458c39f0a349b20ca): normalization/dispatch | Lower priority until parse profiles show material conversion/dispatch costs. Preserve Unicode namespace lowering and pool identifiers; direct variable names and namespaces have different grammar. Measure full reload, not only string-loop throughput. |
| P05 | [ace1f29f](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/ace1f29f3c067dcac736a2a8932dc35d31d03d13): compatible source-view reuse | Manual port after flags and ownership checks. Verify the view belongs to the selected resource and supports the destination's required view interface. A custom resource's SRV must never be blindly cast to a UAV. Measure avoided Create*View calls and reference-copy CPU cost. The feature is separable from the broader new syntax/cache policy. |
| P06 | [fb2b5b0d](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/fb2b5b0dc3f0456a431ec0d9a8211ba7442664d5) + [034e2875](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/034e2875ff7b04eefa75a312144cd5338d8c6cce), with selected parts of [48f3b6db](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/48f3b6db03814bac65b0ac990e0a26446c3bd73f): candidate cache | **Manual redesign before adoption.** Preserve current match sets and address resolution invalidation, hash/fuzzy policy and missing-handle fallback below. Do not take [c1378828](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/c1378828238f165cec8efbaf78cab985373e7bbd) behavior unchanged. Measure rebuild/lookup separately plus memory/resource churn. |
| P07 | SRV batching base `9f04a753`, `c3ae4fd5`, `c5d0caa7`, `ef97cf72`, dynamic exclusion `84c4948f` | **Defer until the null/order hazards below are closed.** A conservative ref-only port is smaller than importing all-option/dynamic/range features. Measure eligible-run frequency and getter/setter call reduction as well as total wrapper time. |
| P08 | [0c7905e2](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/0c7905e23d783d2f29992cb68caf321ff46226bf) + [1fba754a](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/1fba754a6bf50e5fbeb01e3087353b296595661e): explicit CPU cache policy/cache-first store | Optional after C02 and real cache/readback evidence. Port all relevant write/copy/clear invalidation hooks together. Preserve local deep-copy ownership and L2 hash/multi-page invalidation fixes; latest upstream still has a different pointer-adoption contract. Record hits/misses, GPU stalls, write-side copies, RAM and full frame tails. Default to no extra tracking for mods that do not need it. |
| P09 | Corrected conditional batching `4e049386`, `2811c279`, `9110defe`, `8fee3c16`, `b4fc34cf`, `2979395f`, `09e3266b`, `189668cf` | Only after P07. The original PR66 series was reverted; use the final PR68 semantics, including `BatchTarget`, pre/post phase, pipeline-reading exclusions and missing-else seeding. This does not close the additional null/order cases found here. |

Instrumentation can precede these changes: `9b3d52f2` / `6cbdf23f` expose fuzzy
matching costs, `76a381d6` splits candidate rebuild/lookup, and `a6b30761`
registers profiled command lists once per collection generation. Keep this as a
separate profiling-overhead improvement; profiling-off frame traces remain the
performance outcome. None of P01–P09 has a measured game improvement in this
audit. Existing local region-cache microbenchmarks do not establish those gains.

## 5. Candidate-cache compatibility and newly found invalidation hazard

Final upstream always iterates hash and fuzzy candidates. EDHM returns after
successful hash matches unless `fuzzy_match_alongside_hash=1`; this affects
`filter_index` and HUD colors. Preserve that decision after draw-specific hash
filters, rather than deciding from the existence of a hash candidate alone.
Cache descriptor candidates, then apply draw-count/offset predicates per call.
Region-hash paths must use region-hash matches, not cached whole-resource hashes.
Untracked resources, including custom resources/backbuffers, still need EDHM's
descriptor fallback; latest upstream intentionally returns no candidates there.
[Candidate implementation](https://github.com/SpectrumQT/XXMI-Libs-Package/blob/065dc746c21c71ef48bbc93cb6b90b3dcdf766e6/DirectX11/ResourceHash.cpp#L1805).

An additional invalidation dependency was missed by the earlier optimization
summary: fuzzy predicates are not all descriptor-only. `eval_field` reads
`G->mResolutionInfo.width/height` for `res_width`/`res_height`. Candidate matching
is cached until config invalidation, but `ResizeBuffers` and device/swapchain
creation paths update resolution without calling candidate invalidation. A
resource surviving a resolution change can therefore retain stale eligibility.
This is a static deduction from the following reads/writes, not a reproduced
Elite regression. Key the cache by a generation covering effective resolution,
invalidate on each effective-resolution change, or keep resolution-dependent
rules as live predicates.
[Resolution-dependent predicate](https://github.com/SpectrumQT/XXMI-Libs-Package/blob/065dc746c21c71ef48bbc93cb6b90b3dcdf766e6/DirectX11/ResourceHash.cpp#L1390),
[ResizeBuffers update](https://github.com/SpectrumQT/XXMI-Libs-Package/blob/065dc746c21c71ef48bbc93cb6b90b3dcdf766e6/DirectX11/HackerDXGI.cpp#L716),
[config invalidation](https://github.com/SpectrumQT/XXMI-Libs-Package/blob/065dc746c21c71ef48bbc93cb6b90b3dcdf766e6/DirectX11/IniHandler.cpp#L3520).

Candidate pointers must be built and read while the local lifetime/config lock
is held; reload must invalidate before destroying override storage. Also verify
resource release/pointer reuse, hash mutation, backbuffers, unchanged textures
across resize, first frame, theme reload, priority ordering and opt-in dual
matching. A reduction in fuzzy evaluations is not acceptable if the result set
changes outside the requested policy.

## 6. SRV batching hazards requiring resolution before port

The latest code has good safeguards: fixed slots, same stage/direction,
non-pipeline bind sources, per-operation resolution, prefetch for `unless_null`,
conditional branch-target access, pre/post separation and refusal to fold
pipeline-reading conditions. These safeguards do not prove all-option batching
preserves behavior. The following are source-derived review findings, not
executed reproductions.

**Failed-copy nulling:** `ShaderResourceBindBatch::run` initializes its entire
view array to null and fetches current bindings only when a branch/option may
leave one alone. A copy that fails `RecreateCompatibleResource` returns without
`SetOrDeferResource`, so `binding.assigned` remains false. The bulk setter still
writes the null array slot. Unbatched `CopyResourceToResource` exits before
setting anything, leaving the previous binding. Preserve old bindings for any
unassigned operation or restrict batching to operations whose outcomes are
represented explicitly. Verify resource-creation failure with preexisting
non-null bindings, separately from intentional null-source assignment.
[Bind loop](https://github.com/SpectrumQT/XXMI-Libs-Package/blob/065dc746c21c71ef48bbc93cb6b90b3dcdf766e6/DirectX11/CommandList.cpp#L12789),
[early failure](https://github.com/SpectrumQT/XXMI-Libs-Package/blob/065dc746c21c71ef48bbc93cb6b90b3dcdf766e6/DirectX11/CommandList.cpp#L12584).

**Gap-group side-effect reordering:** `emit_slot_batches` sorts unique slots and
emits contiguous groups in numeric order. It keeps INI order within each group,
but can reorder operations between groups. Eligibility accepts all copy options,
including `set_viewport`. Consider this qualifying three-operation run, with
textures A/B/C having different dimensions:

```ini
ps-t2 = copy set_viewport ResourceA
ps-t3 = copy set_viewport ResourceB
ps-t0 = copy set_viewport ResourceC
```

Unbatched execution ends with C's viewport. Numeric gap grouping emits t0 first,
then batches t2/t3, ending with B's viewport; the shorter output list is accepted
by the optimizer. Exclude side-effecting options or preserve global operation
order while batching setters. Merely keeping duplicate-slot order inside one
batch does not resolve this case.
[Gap grouping](https://github.com/SpectrumQT/XXMI-Libs-Package/blob/065dc746c21c71ef48bbc93cb6b90b3dcdf766e6/DirectX11/CommandList.cpp#L13137),
[eligibility](https://github.com/SpectrumQT/XXMI-Libs-Package/blob/065dc746c21c71ef48bbc93cb6b90b3dcdf766e6/DirectX11/CommandList.cpp#L12867),
[viewport side effect](https://github.com/SpectrumQT/XXMI-Libs-Package/blob/065dc746c21c71ef48bbc93cb6b90b3dcdf766e6/DirectX11/CommandList.cpp#L12654).

Additional required cases are duplicate slots, gaps, null/`unless_null`, explicit
null, missing else, slots above zero, pipeline resource hazards, all shader
stages, copy versus ref options, pre/post and deferred contexts. A safe initial
EDHM subset can reject uncertain operations rather than adopting every newer
language feature to reach an optimization.

## 7. Conditional follow-ons and deliberate exclusions

- **Buffer regions:** Port `532a37c5` together with refinement `c226158e`, and
  the C06 stride fix, when repairing already exposed `->Region(offset,size)`
  operations. Current code only extracts explicit region length in the ref
  branch and can confuse count/end or visible CB-window/backing size. Preserve
  an explicit byte count through destination recreation, clamp against backing
  extent and retain local overflow checks. Also reject source offsets beyond
  the buffer before subtracting: an unsigned `size-offset` expression alone
  cannot establish safety. These are correctness ports; dynamic slot/range
  syntax is not required to repair the existing region operation.
- **Stream output:** Evaluate `1d9403c0` and `75e52ba6` together; use appropriate
  reference-array ownership from `7bea1274` without pulling unrelated range
  syntax. Non-target SO slots must preserve append positions rather than reset
  to zero; release getter references. Prioritize if a mod uses SO targets.
- **Namespace isolation:** `4cd308a5` rejects replacing a nonempty command list
  from another namespace. Useful guardrail, but a compatibility policy change;
  verify current EDHM command-list libraries before enabling it.
- **Input layouts:** Do not import `64bcd002` as a replacement for EDHM's real
  layout/side-car boundary. Upstream strong-cache fixes concern wrapper/device
  caches that EDHM deliberately does not use. A future deduplication design
  must still return genuine layouts and prove reference/lifetime behavior.
- **Shader bytecode introspection:** New reader/member getter architecture
  `80481d19`, `fdc2539c`, `3f2f3700`, `c530d130` is a feature, with locking fix
  `3b53481c` mandatory if it is adopted. Defer without an EDHM authoring need;
  parsing new declarations during CreateShader is not a proven speedup.
- **Timing:** `e850bd7b`, `47428406`, `be6bd4ea` provide finer TIME, FRAME_TIME
  and FPS operands; `ca4d732e` fixes frame-rate-dependent spatial radius.
  Port the spatial behavior independently if needed, with a deliberate time
  unit contract. Preserve EDHM's wrap-safe clock, polling and log batching.
- **Logging/input/PNG:** `440ac3a9` changes to log-level policy and needs
  initialization fix `a756fdec`; `3102185e` input toggle needs default fix
  `45a8ff4e`; PNG sRGB detection ends disabled by `d492122f`. These are optional
  user-visible contracts. Preserve current EDHM defaults rather than treating
  them as performance upgrades.
- **Excluded build components:** Injector, compiler/DirectXGI proxies, full
  upstream solution, generated metadata scheme and unrelated packaging workflows
  remain outside the slim EDHM runtime. Latest upstream dependency or project
  changes are not authorization to restore those graphs. Keep local v143,
  strict C++17, `/GS`, analysis and release conventions.

All short IDs in this section identify commits in the pinned XXMI history;
their full links are listed below for review. A fresh upstream ref is evidence
for comparison, not a reason to merge all upstream code into EDHM.

## 8. Concrete delivery order and acceptance

1. **Small correctness patch set:** C01, C02 with local lifetime/type/range
   corrections, C03, C04–C06, C07–C08. Keep changes separately reviewable.
2. **Persistence patch:** C09 as one final-state manual port; avoid interleaving
   unrelated logging/timing migration. Revalidate no lost settings across theme
   reload/autosave/failure paths.
3. **Existing region/SO semantics:** select the applicable fixes from section 7
   before features depend on them. No dependency on new slot-range syntax.
4. **Small performance patches:** P01, then P02/P03 separately; P04 only with
   parse-profile evidence. Keep baseline data and patch hashes.
5. **View reuse:** P05 with matching resource/kind and COM lifetime validation.
6. **Experimental candidates:** P06 after compatibility/resolution invalidation
   is solved; P07 after failure/order behavior is solved. Keep them independent
   so their effects can be measured separately.
7. **Workload-driven expansion:** P08, then P09 after its base is stable, only
   where actual mod command lists/cache misses justify their additional surface.

For each implemented item record: exact source SHA and patch; affected setting
and real workload; correctness evidence; strict x64/Win32 Release build and
focused checks; warm/cold startup or frame metric as appropriate; raw repeated
observations and noise; memory and write-side costs; Linux/Wine and native
Windows results separately. The repository testing guidelines require reviewing
the proposed observable behaviors before writing new tests and confirming that
each added test detects a deliberate behavior break. No tests were added here.

Do not label a skipped, unsupported, unused or unmeasured item as zero gain.
Do not add isolated percentages across interacting cache/parser/batching changes.
For a normal EDHM play workload, keep hunting, dumping and region tracking off
unless the mod requires them; measuring an optional authoring feature does not
establish faster ordinary play.

## Additional exact commit links

| Commit | Change |
|---|---|
| [09e3266b](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/09e3266b64f3748dd904942b8b949a171f7590fb) | Conditional batching: keep an unbatched chain as its own if command |
| [189668cf](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/189668cfe22db557329d9e4a50be17806ca7d8a1) | Conditional batching: log when no branch is taken |
| [1d9403c0](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/1d9403c0c1b94b8f10b2e87c5060a45ce398661c) | Stream Output Target fixes: offset handling, reference counting, and documentation |
| [2811c279](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/2811c279d5e5f6cd23a4fa15c0527243358acff4) | Batching: read the batch slot through the operation |
| [2979395f](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/2979395f9e446fa89c4156d5538a4c3f4c4eb306) | Conditional batching: skip conditions that read pipeline state |
| [2d6af722](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/2d6af7226c9346a7a8795a76a14a74e5dbe65f2e) | Persist Vars Preservation Code Adjustment |
| [3102185e](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/3102185e7adaa2069397316f8d76df97dc173ff8) | Overhauled user input toggle |
| [326f64cf](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/326f64cf9ab565aa7cd3d50e002270994b8d16c6) | Fixed spatial pool index regression for the closest neighbor path |
| [3b53481c](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/3b53481caaef393c659674f5b1493241d4b94baa) | Fixed `CacheShaderBindings` UB caused by concurrent `mShaderBindingsCache` access |
| [3f2f3700](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/3f2f370065efad66c615821bff95a23a2aca078e) | Moved get_shader_bindings_from_bytecode to ByteCodeReader.h |
| [440ac3a9](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/440ac3a99d20cbcbaf81e08ea4790bea7571e08a) | Overhauled logging options |
| [45a8ff4e](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/45a8ff4e51c08b97fd2c4eaa3d29616e77ac762a) | Fixed `input` option wrongly defaulting to `false` |
| [47428406](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/4742840607b17188953ad74419d4f826c4dfd36a) | Added FRAME_TIME INI runtime paramater |
| [4cd308a5](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/4cd308a59810d983c3a1298464d49c3e96cbd8bf) | Disallowed overriding non-empty CommandList from different namespace |
| [4e049386](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/4e049386b925fb1ef834474bee6023a83ea58dd9) | Batching: name the batch direction instead of a bool |
| [4e7988e7](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/4e7988e768414958df1bdc825231a86105fa84b4) | Fixed identifiers followed by member getter `->` being wrognly consumed by single token parsers |
| [532a37c5](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/532a37c588e18d8e14a4f701804eec544c9d053e) | Fixed `->Region()` size being treated as region end pos by some `CopyResourceToResource()` paths |
| [64bcd002](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/64bcd002316352b9d79884c3d25617f5375fae04) | Fixed input layout COM wrapper issues |
| [6cbdf23f](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/6cbdf23f90e80644928e390579a281fcf9f0e38b) | add fuzzy texture override matching overhead tracking |
| [6d6dfb60](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/6d6dfb609eafef7c66317d50ef11aad3144e66b5) | Change to Persisting Variables bugfix |
| [75e52ba6](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/75e52ba6248dc5a2daf415afc55c6ac95cbf992a) | Rebind the other stream output slots to append, not to offset 0 |
| [76a381d6](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/76a381d6c9d0bed31e86497569f208bc7868420f) | Profiler: split TextureOverride row into candidates rebuild / lookup |
| [79fd6eda](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/79fd6edafee1242581256e6f4f98dd5fdf0f26ed) | modified:   DirectX11/CommandList.cpp 	modified:   DirectX11/CommandList.h 	modified:   DirectX11/IniHandler.cpp 	modified:   DirectX11/globals.h |
| [7bea1274](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/7bea12741433b38e1f312a883e55c35f5553f9b1) | Share the slot array reference counting between the pipeline targets that use it |
| [80481d19](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/80481d190f13d3e717e3655effd0158f6183af84) | Added experimental shader bytecode parsing and caching for CB and SRV declarations |
| [84c4948f](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/84c4948fca067caf3b0dbd0bb51de5fdf457be7d) | Exclude dynamic slot indices from slot batching |
| [8c439438](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/8c439438aab52f907d4a189594258965738a6a3e) | Refactored unknown persistent settings clean up |
| [8fee3c16](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/8fee3c16a500b16a243f6a5de1bdc2f10fbca20d) | Conditional batching: run fetch batches through the matched branch |
| [9110defe](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/9110defe02384fa04c8a6fb468ecfb6c02f50e20) | Conditional batching: fold same-slot chains into bind batches |
| [98d0d98d](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/98d0d98d051d9414843295e25910e88cc8b6f665) | Adjusted FIFO indexed pool to return default variable or resource template on read for missing UID |
| [9b3d52f2](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/9b3d52f2e9249c35cabbd00a4b95b68899ceb042) | add fuzzy texture override matching overhead tracking |
| [9f04a753](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/9f04a753391bc20f655d1e991c088718ac433896) | Batch adjacent shader resource slot binds and fetches |
| [a6b30761](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/a6b30761912f46c3e97e6692d482ca8a217fde9c) | Profiler: register command lists once per collection window |
| [a756fdec](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/a756fdec80100b5d8462a37f34a808f6da2b5f48) | Fixed logging initialization issues |
| [b4fc34cf](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/b4fc34cf26942745860ad64f2d036acba24ed8d2) | Conditional batching: seed slots a branch may leave alone |
| [be6bd4ea](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/be6bd4eae63273e49bbff62a5e90e70e73597382) | Added `FPS` INI runtime paramater |
| [bf929475](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/bf92947536b4b620ccb0d18cfc0881b51ffb65a5) | Made "failed" state recoverable for CopyCommandListCommand::run and PoolCopyOperation::CopyPoolToPool |
| [c05b4cae](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/c05b4cae770bcba965e5a1d8511ea8a1f7446080) | Persistent Variables Preservation Tweak |
| [c226158e](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/c226158eb16589f79d0041d003defc7944f45a00) | Fixed `->Region()` size being clamped to SetConstantBuffers1 CB "window" size, fixed possible SpecialCopyBufferRegion offset overflow |
| [c3ae4fd5](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/c3ae4fd5cca9a2830b4893472505e26696bf1b4b) | Batch slot binds and fetches regardless of copy options |
| [c530d130](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/c530d130bcadfa376b24261cac0d7e91b208730c) | Refactored shader bindings parser |
| [c5d0caa7](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/c5d0caa7c166e9689366101864993caaeebbb8dc) | Tidy slot batch merging and comment it |
| [ca4d732e](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/ca4d732eaa99ccfea4461d7d7027291a7623ab98) | Fixed `pool_spatial_radius` being FPS-dependent |
| [d492122f](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/d492122f8e70e7a8b131a81d14cea97a1cdadb45) | Made sRGB PNG chunk detection optional and disabled by default |
| [dd1a1a57](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/dd1a1a5770e8905c3a8a5ef71c0569224a787666) | Additions to handling Unrecognized Persist Vars |
| [e850bd7b](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/e850bd7b8b6031fc325734664029678ec8302d97) | Improved TIME precision using microsecond-resolution timing |
| [ef97cf72](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/ef97cf7239aa2642b50de3fd85040028282056fa) | Rename seed_with_current to prefetch_current_bindings |
| [efb0c050](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/efb0c0508eecf1dfa0104835154c6d40b05a0f1e) | Overhauled unknown persistent settings notification and logging |
| [fdab51e2](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/fdab51e2f26438a3e9ea6b3dcc34c5a84de550f5) | Fixed square and round brackets being wrongly treated as part of identifier |
| [fdc2539c](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/fdc2539cc624f4708abfdba719b98461d5120491) | Moved shader bytecode reader code to separate file |
