# Upstream intake audit for the next EDHM release

**Verified:** 2026-11-12. **Local source:** `34d92f27f588bbc3f72320c0fa17b674638698c6`.

## Findings and decision

The XXMI reference branch has been fast-forwarded to **v1.2.3 / `065dc746`**, both
locally and on `origin`. The current working branch, its HEAD, pre-existing
changes, release source and installed game files were preserved. Original
3Dmigoto's tracking ref has also been refreshed to **`8f329bd9`**.

For the next release, take a small correctness stack first, then selected
load/reload optimizations. The strongest narrow candidates are empty-parenthesis
rejection, leading-dot numeric literals, pool proxy reset, resource-reference
flag propagation, pool metadata lookup and structured-to-constant-buffer stride
cleanup. Adapt the structured-buffer `store` fix with additional local lifetime
and type checks. These fix real source-level gaps in the current runtime; they
are not claimed FPS improvements.

Do not blindly cherry-pick the newer TextureOverride candidate cache or SRV
batching. Besides the known EDHM hash/fuzzy and layout differences, this review
identified source-level resolution invalidation, failed-copy and ordering
hazards that need addressing before adoption. Original 3Dmigoto has no mandatory
ordinary-play performance patch in the reviewed range; its unported decompiler
extension is incomplete and belongs in a separate tooling change.

This is a source and integration audit. No candidate was applied, built or
benchmarked in Elite during this task. Rankings reflect correctness impact,
EDHM relevance, dependency size and work removed. The previously agreed
**XXMI v0.9.8 performance comparison remains unchanged**; moving the reference
branch does not change that benchmark's pinned commits.

## Reference update and exact comparison ranges

| Ref or source | Before | Verified after |
|---|---|---|
| Local `xxmi-base` | `af232a90fda4e5fc8799f854f07787a3bc33bcbe` | `065dc746c21c71ef48bbc93cb6b90b3dcdf766e6` |
| GitHub `origin/xxmi-base` | `6bf6a746a198c82fb34dbd336cac7ad8e0f4ddc9` | `065dc746c21c71ef48bbc93cb6b90b3dcdf766e6` |
| `xxmi/master` | `326f64cf9ab565aa7cd3d50e002270994b8d16c6` | `065dc746c21c71ef48bbc93cb6b90b3dcdf766e6` |
| `3dmigoto/master` | `4ce5f2f72b2777223d2e809dcdafec514ac98295` | `8f329bd94fecc9bbcb9211ffd42a95dd7fe6b43e` |
| Current working HEAD | `34d92f27f588bbc3f72320c0fa17b674638698c6` | Unchanged |

Both prior XXMI reference tips were verified ancestors of the new tip. The local
reference moved 296 commits; the GitHub reference moved 106. Ordinary
fast-forward fetch/push operations were used; no forced branch reset or checkout
was needed. The final GitHub ref was read back with `git ls-remote` and matched
the upstream/local ref. The other upstream repositories were read only.

The important intake baseline is the **actual selective port through
`326f64cf`**, recorded in [the port notes](../patches/0002-xxmi-since-2026-07-17.md).
That is slightly later than the v0.9.8 tag used for the historical benchmark.
From `326f64cf` to latest XXMI there are **236 commits / 178 non-merge commits**,
with **68 changed files, 10,451 insertions and 1,257 deletions**. Counts describe
history, not missing features: local ports can implement equivalent behavior
without preserving upstream SHAs.

Local v0.1.3 was released on 2026-09-21; its runtime source matches current HEAD.
Original's latest decompiler addition was written/merged in August. It is newly
identified missing work relative to the imported lineage, not a change written
after September's local release. The broader XXMI range likewise contains older
unported work as well as newer changes. Published versions and branch tips were
checked independently. [XXMI v1.2.3](https://github.com/SpectrumQT/XXMI-Libs-Package/releases/tag/v1.2.3),
[original tip](https://github.com/bo3b/3Dmigoto/commit/8f329bd94fecc9bbcb9211ffd42a95dd7fe6b43e),
[original release/source accounting](upstream-original-next-release.md#1-live-release-and-source-verification).

## Recommended intake order

Each row is a separate reviewable change. A dependency listed in the row belongs
with that change; do not use a whole upstream branch merge to obtain it.

| Order | Upstream change | Why it is useful | Integration decision |
|---|---|---|---|
| 1 | [c4e3a449](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/c4e3a4499d05268bfef3896ff4eeee8ed18a34be), reject empty parentheses | Current operator transformation lacks the empty-token guard. Invalid expressions should produce an INI error rather than invalid iterator arithmetic/crash. | Small manual port into `transform_operators_visit`; preserve normal syntax diagnostics. |
| 2 | [a9d7c89b](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/a9d7c89b1ba957ac8a65a8bbf72acefa86eec544), accept `.123` literals | Restores valid leading-dot float syntax; current tokenizer enters numeric parsing only for a digit. | One-condition manual port, keeping wide-character-safe classification and existing numeric validation. |
| 3 | [ad264db8](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/ad264db86ee7d4870215edce95287614713f2a41), `PoolFoo = null` | Restores pool proxy reset routing to the already-existing empty-source handler. | Unmodified patch passes the read-only applicability check. Still validate behavior before shipping. |
| 4 | [038af009](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/038af0099ed5be1c33fe1e8a7485fffd03ed704a), reference-chain flags | One-hop parse-time propagation can create an underlying resource without flags required by later aliases/slots, depending on parse order. | Port the complete three-file change: collect edges, clear before resource teardown, resolve after all command lists are parsed. Preserve local ownership and reload order. |
| 5 | [7d2c4705](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/7d2c47051f1cceb6abe4294e391bcc526d678e50), pool bind flags from template | A metadata query should not resolve/create an element or postpone expiration before `unless_null` can cancel the copy. | Small semantic port of `BindFlags`; keep explicit static-element behavior. No general pool-speed claim. |
| 6 | [6b2d803f](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/6b2d803f50892ee87685e80ce4f561bdeb12f173), clear CB stride | A buffer recreated as a constant buffer must not report the source structured-element stride through later metadata reads. | Unmodified patch passes the read-only check. Preserve current size/bounds protections. |
| 7 | [24e2cff7](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/24e2cff7b2bbfde31a8882c06d178fc008152997), structured-buffer `store` | Current code copies an arbitrary four-byte source interval; structured-buffer copies require whole structure elements. Upstream adds aligned copy ranges, internal value offset and bounds checks. | Adapt, rather than copying unchanged: validate that the source is a buffer, validate the evaluated index before integer conversion, and balance both source-view and resource references on every exit. |
| 8 | [7ea5e930](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/7ea5e9306c51234301781d226766c01540270c32), constants compaction | Replaces repeated vector erase/shifting with one stable compaction pass. Particularly relevant to large EDHM constants/configuration loads. | Manual port with unchanged declaration/initialization order and intentional invalid/duplicate-global handling. Measure load/reload milliseconds. |
| 9 | [06962f17](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/06962f170a36eeaf9bfa5cd9d7ce4b748fbf56cd) + [8d156caa](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/8d156caad87180bf038f0892451a6a6a91fcf264), current-section reuse | Avoids repeated INI section-map lookups on each line. | Port the pair together, retaining duplicate-section, include and UTF-8/path behavior. |
| 10 | [d8937803](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/d89378035550033fc2949d39813ac852de1af6b6), static-condition memoization | Avoids repeatedly evaluating the same static condition during command-list optimization passes. | Port implementation and new state fields together; include its small follow-up cleanup where appropriate. This is loading/optimization work, not an established per-frame win. |
| 11 | [abaed4e4](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/abaed4e420f5aec115a169740db74876874ed629), early buffer exit | Skips irrelevant hash-contamination lock/map work for buffer resources. | Narrow manual port if tracking is used; the extra `GetType` call and tracking-off control must be included in measurement. |

Orders 1-7 form the preferred correctness wave. Orders 8-11 form a separate
performance wave; their order can change if an actual profile identifies a
dominant path. Operator-category parsing/direct tree traversal
([1ff00d94](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/1ff00d94096fffef76122bde8c0c88eb1dc1253b),
[87fa9479](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/87fa9479cfc8e1f5c1ae9dcdcd3cb2a4d3a336d8))
and variable normalization are worthwhile later parser candidates, after the
small correctness fixes and simpler load-time changes are stable.

Another narrow compatibility candidate is
[f9d665e2](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/f9d665e220d8551015b88b5bb22976929e509bdf):
pool-variable identifiers should reach the resource-target parser before the
regular-variable grammar rejects valid pool names. Include it when supporting
hyphenated, Unicode or namespaced pool identifiers; preserve ordinary-variable
fallback. It is outside the 32 patch checks below.

The installed include-tree scan found many resource-reference assignments but
no `store`, spatial-pool or ShaderRegex declarations in the nine INIs scanned.
This is a lexical observation excluding comments and disabled paths, not an
execution trace or proof of every loaded setting. It supports prioritizing core
parser/reference behavior over enabling expensive optional features; it does
not establish how often a candidate runs in normal play.

## Adapt later or defer

### TextureOverride candidate caching

[fb2b5b0d](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/fb2b5b0dc3f0456a431ec0d9a8211ba7442664d5)
and its locking refinement can reduce repeated matching, but the latest upstream
implementation is not a safe direct port into EDHM:

- It combines hash/fuzzy candidates and drops matching for resources without
  tracked handle information. Preserve EDHM's hash-first default, explicit dual
  matching option and untracked-resource fallback.
- Fuzzy predicates can read `G->mResolutionInfo.width/height` through `res_width`
  and `res_height`. Upstream caches those results per resource, but resolution
  updates do not visibly invalidate the cache; the explicit invalidation call
  is attached to configuration parsing. Persisting resources can therefore
  retain stale candidates after a resolution change. An EDHM implementation
  needs a resolution generation/key or selective re-evaluation.
- Keep draw-dependent filters outside cached descriptor matching; make reload,
  hash changes, resource destruction and synchronization part of the design.

This is a promising performance project after instrumentation, not a simple
maintenance cherry-pick. The resolution issue is a source-level finding, not a
reproduced in-game failure. [Detailed evidence](upstream-xxmi-next-release.md).

### Slot binding and fetching batches

[9f04a753](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/9f04a753391bc20f655d1e991c088718ac433896)
and later changes can reduce D3D setter/getter calls. Two remaining static
hazards prevent recommending an unchanged import:

- The bind batch starts with null views unless current-binding prefetch is
  selected. A copy can fail before assigning its deferred result. Unbatched
  execution leaves the destination binding alone in that failure path; the
  batch's final bulk setter can instead clear it.
- The optimizer sorts slot numbers and emits separated ranges in slot order.
  With side-effecting copy options such as `set_viewport`, operations from
  separate ranges can run in a different order from the INI and leave a
  different final viewport. Eligibility needs narrowing or execution order must
  be preserved independently of the final bulk bind.

These are source-derived behavioral examples; no DX11 runtime reproduction was
performed. Address them before accepting API-call reductions. Conditional
batching is a further dependency layer: use only the corrected PR68 series,
not reverted PR66, and validate missing-else, nonzero slots, pre/post phases and
pipeline-reading conditions. [Detailed source paths and examples](upstream-xxmi-next-release.md).

### Persistence, source-view reuse and cache-first store

The unknown-persistent-variable lifecycle is useful, especially when mods are
temporarily absent. Port the final retention/removal/reload behavior as a
cohesive change rather than cherry-picking the first preservation commit and
dragging in upstream logging/initialization policy. Existing dirty-flag fixes
already cover part of the history.

[ace1f29f](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/ace1f29f3c067dcac736a2a8932dc35d31d03d13)
can avoid view creation where the original view is compatible. Evaluate it
separately with correct COM interface/resource checks. Cache-first `store`
([1fba754a](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/1fba754a6bf50e5fbeb01e3087353b296595661e))
and configurable buffer-data caching need demonstrated readback pressure: they
can add write-copy work and RAM cost. Neither should be enabled globally just
because a hit path is fast. Fix the existing store lifetime/type issues first.

Namespace isolation [4cd308a5](https://github.com/SpectrumQT/XXMI-Libs-Package/commit/4cd308a59810d983c3a1298464d49c3e96cbd8bf)
applies textually, but forbidding existing cross-namespace command-list
replacement changes behavior. Audit the shipped/mod INIs before opting into
that stricter policy; clean patch application alone is insufficient.

### Existing buffer-region and stream-output semantics

The `->Region(offset,size)` feature is already exposed locally. Port
`532a37c5` with `c226158e` when correcting its copy-size versus end-offset and
CB-window/backing-size behavior; a new dynamic slot-range language is not a
prerequisite. Preserve local overflow checks and reject offsets beyond the
buffer before unsigned subtraction. If SO targets are supported by the target
mod, evaluate `1d9403c0` with `75e52ba6` and the required reference-array ownership:
preserve untouched slots' append positions and release returned references.
These can be separate correctness follow-ons, without enabling a broader cache
or batching architecture. Exact source links and acceptance cases are in the
[conditional follow-on inventory](upstream-xxmi-next-release.md#7-conditional-follow-ons-and-deliberate-exclusions).

## Original 3Dmigoto: useful differences from XXMI

After original 1.4.9 there is one non-merge change, plus its merge:
[f63fe792](https://github.com/bo3b/3Dmigoto/commit/f63fe792052059f1e58ee3d9f0f40967c430d68b)
adds raw-buffer/Texture1DArray HLSL decompilation. It is absent in both XXMI
v1.2.3 and current EDHM. However, upstream discussion does not establish a
successful shader round trip, and source review identifies incomplete raw-load
component/operand handling. Treat it as an optional adapted shader-authoring
feature, not a ready runtime-speed patch.

Optional original-only header hygiene is limited to
[32d6670d](https://github.com/bo3b/3Dmigoto/commit/32d6670d079c195cbb70a35d2dcf6c273fa36ec3)
(decouple the context-hook header) and
[91986f01](https://github.com/bo3b/3Dmigoto/commit/91986f015908592c60b16f911a876b5f508fef02)
(explicit shell API include). These do not justify importing original's larger
project/dependency reshuffle. The [original intake appendix](upstream-original-next-release.md)
accounts for all 60 original-only non-merge commits and the known equivalents.

## Already implemented: do not cherry-pick again

| Upstream item | Current local evidence / decision |
|---|---|
| XXMI `7ff1fe7b`, shader error blob release with logging off | `ReplaceHLSLShader` already releases the blob independently of logging, with additional checks. Keep current code. |
| XXMI `57a1b6a2`, spatial `offset_y` maximum | Local nested maxima already include the required offsets, with stronger size/overflow handling. |
| XXMI `60f2cd44`, dirty flag preservation | Local variable-update paths already accumulate changes with `|=` rather than overwrite prior dirtiness. |
| Original `dafa4946`, ShaderRegex error cleanup | Present through XXMI `b6dc850f`; local code removes registered pre/post lists before deletion. |
| Original `b942d0a3`, D3D11On12 forwarding | Present through XXMI `f46dbbf1`, with stronger local missing-export/output guards. |
| Upstream layout-wrapper lifetime fixes / device cache | Local side-car design has a different contract: Elite receives genuine D3D layout pointers. Do not replace it with upstream wrapper returns. |
| Original v143/C++17/project upgrades | Local strict build and quality setup already meet the relevant toolchain requirements; retain the slim solution. |

Semantic equivalence was checked in the current functions, not inferred solely
from ancestry. These skip decisions refine the earlier broad upstream shortlist.
[XXMI analysis](upstream-xxmi-next-release.md), [original analysis](upstream-original-next-release.md).

## Port readiness and verification limits

Read-only `git apply --check` checks were run for 32 selected commits, both
normally and with `--ignore-space-change`. Only `ad264db8`, `6b2d803f` and
`4cd308a5` passed unchanged. Most recommendations therefore need local ports or
conflict resolution because functions, ownership, formatting and parser
interfaces have diverged. This does **not** predict exactly how a three-way
`git cherry-pick` would merge, prove semantic correctness, or replace a build.
No apply/cherry-pick mutation was performed.

For implementation, keep each change isolated and run the repository's strict
MSVC/Sonar-enabled native checks and focused behavior validation. Important
acceptance cases include invalid/valid expressions; null/reset/expiration
behavior; reference chains in different parse orders; structured store offsets,
bounds and failures; constant-buffer metadata; persistence removal/reload;
resolution changes; and batched/unbatched output equivalence. Performance runs
should use Release without the debug layer and distinguish startup/reload time
from steady frame cost.

This task ran no new runtime tests or performance measurements. Static hazards
are identified as such; measured speedups are not inferred from commit titles.
The [machine-readable evidence](upstream-next-release-data.json) records ref
movement, source pins, the selected patch checks and the scoped INI scan.

## Detailed research

- [XXMI candidate and dependency audit](upstream-xxmi-next-release.md)
- [Original 3Dmigoto cross-repository audit](upstream-original-next-release.md)
- [Historical performance audit](performance-audit.md), retaining the agreed v0.9.8 benchmark baseline
