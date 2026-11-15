# Inherited 3Dmigoto / XXMI architecture and optimization audit

Audit date: **2026-11-12**. Audited EDHM source: **`34d92f27f588bbc3f72320c0fa17b674638698c6`**. This is a source audit of the inherited runtime, with the local EDHM behavior checked explicitly. It complements the [upstream release audit](audit-upstream-baselines.md) and the main performance audit. **No game-frame-time improvement is measured in this appendix.** Every timing gain below is unknown unless the main report supplies a matching measured experiment.

## Brief

The strongest inherited optimizations avoid whole categories of work: shader disassembly for patternless ShaderRegex rules; GPU readback when CPU snapshots or region hashes are reusable; redundant staging-buffer creation; and CPU copying of a buffer that can be adopted into the cache. These paths are conditional. EDHM's shipped template disables region hashing, so region-cache improvements cannot explain normal-play gains unless that setting was changed. The indexed custom-pool system is mainly new functionality for authors and is not an automatic speed increase for an unchanged EDHM installation.

The most immediate source-level performance candidate is a duplicate `BinaryToAsmText` call in the ShaderRegex pattern path. Removing the redundant successful call would remove **one of two disassembly invocations** for each affected cache miss. That does **not** establish a 50% shader-load or frame-time improvement. Conversely, current EDHM deliberately does **not** retain XXMI's device-level input-layout deduplication cache: it uses real D3D objects with metadata side-cars to preserve Elite compatibility. [Current ShaderRegex path][s-context], [current layout creation][s-device].

## Baselines and attribution

| Baseline | Immutable revision | Meaning for this appendix |
|---|---|---|
| Original latest stable, `1.3.16` | `cbc4993ca67b6d639e06c578d4e3dc8b3780243f` | Long-lived stable comparison; includes fewer of the original project's later changes. |
| Original latest published including prerelease, `1.4.9` | `4ce5f2f72b2777223d2e809dcdafec514ac98295` | Also examined so a change already in original's newest package is not called an XXMI invention. |
| Locally imported XXMI release, `v0.9.8` | `f8e7118a1220e43546e546efa31892b8304358df` | Boundary for inherited XXMI inventory. This is **not** the latest live XXMI release. |
| Current local EDHM | `34d92f27f588bbc3f72320c0fa17b674638698c6` | Retention/adaptation checked against actual current code, rather than assuming every upstream commit survived. |

Release publication metadata and latest XXMI gaps are maintained in [the separate upstream audit](audit-upstream-baselines.md). A Git range lists ancestry differences, including merged original-project work; it does not prove authorship or exact runtime behavior. The catalogs below identify the original-project changes separately. Source/release timestamps alone cannot establish when an EDHM user adopted a change.

**Already present at original `1.3.16`:** command-list expression trees and constant folding (`optimise_command_lists`, `CommandListOperator::optimise`); the original copy-operation `ResourcePool` cache; compiled custom-shader caching; ordinary shader replacement/ShaderRegex caching; and draw-info filters on TextureOverrides. Their existence must not be counted as a new XXMI optimization. XXMI adds extensions to these systems. [Original command-list implementation][original-command], [original resource matcher][original-hash].

## Optimization catalog

All entries have **unmeasured application performance** in this appendix. “Avoided work” is a code-level description, not a measured millisecond or FPS result. Most entries are incremental versus both original releases unless their baseline column says otherwise.

| ID | Mechanism and before → after | Current activation / retention | Avoided work; costs and limits | Primary evidence |
|---|---|---|---|---|
| I01 | Strip stereo rendering machinery: stereo handles, stereo parameter textures/binding and per-draw stereo override operations are removed from the 2D fork. | Retained architectural choice. Legacy `sli`, `stereo_active`, `stereo_available` expressions report false. | Removes stereo-only work and objects; original already skipped many operations without stereo enabled, so saved lines of code cannot be translated to frame-time gain. NVAPI profile support remaining in the tree is not evidence that stereo remains supported. | [Stereo strip][c-1921b4fa]; `HackerDevice`, `HackerContext`, `HackerDXGI`, `Override`, `profiling`. |
| I02 | INI getters replace exception-driven `.at()` misses with explicit `.find()` lookups; filename glob conversion moves from a `wstring_convert` codec object to Win32 UTF-8 conversion. | Used while parsing/reloading configuration, not per draw. Current conversion has later EDHM hardening. | Avoids exception construction/unwinding for absent section/key defaults; conversion cost should be measured separately. No guarantee of measurable steady-state FPS change. | [INI getter/conversion change][c-beb7f481]; [current INI parser][s-ini]. |
| I03 | `TIME` expression reads a cached global time instead of calling `GetTickCount` at each evaluation. | Retained. Current EDHM refreshes `G->gTime` from `GetTickCount64()` during Present actions. | One clock sample is shared by consumers during a frame; changes within-frame time resolution. Its value depends on the number of `TIME` operands evaluated. Delayed initialization and autosave in the same commit are separate lifecycle changes. | [Time caching commit][c-46b9540c]; [current Present actions][s-dxgi]. |
| I04 | Shader model is read from DXBC bytecode and cached; ShaderRegex rules without Patterns link their command lists without disassembling the shader. | Retained; relevant on deferred shader analysis/cache misses when no applicable pattern rule needs assembly text. | Can avoid `BinaryToAsmText` entirely. Pattern rules still require it; warmed regex-cache results already had a shortcut in original code. Current pattern path also contains the duplicate call described below. | [Model/regex shortcut][c-6ebd2733]; [current context][s-context], [current regex implementation][s-regex]. |
| I05 | Region-hash matching first narrows candidate overrides by `match_index_count` / `match_vertex_count`; hashes only when candidates remain. | `track_region_hashes=1` and relevant IB/VB targets; current shipped setting is `0`. | Reduces unnecessary region CRC and possible cold readbacks. The new region feature itself adds overhead versus the ordinary original whole-resource matcher. No candidates must not be treated as a failed full-resource hash. | [Prefilter overhaul][c-fcf65acd]; `ResourceCopyTarget::FindTextureOverrides`, `get_fuzzy_matches_by_draw_info`. |
| I06 | Reorder `matches_draw_info` checks so index count / first index / vertex count are checked earlier. | Retained ordinary and region matching. | Earlier rejection may save later comparisons when those fields are selective; gains depend on actual rejects and branch behavior. It is a micro-optimization, not a new matching feature. | [Check-order change][c-4eba8754]; [current matcher][s-hash]. |
| I07 | Introduce CPU snapshot + per-resource region-result cache + global fast lookup: L1 data, L2 `(offset,size)`, L3 `(resource,offset,size)`. | Region queries / `track_region_hashes`; current EDHM adds synchronization and correctness fixes. | Hits avoid CRC, handle-map lookup or GPU readback depending on level. Memory growth, invalidation and locking are real costs; global L3 may contend across contexts. L3 is cleared on frame/state changes as implemented. This is an optimization **within a new feature**, not proof region hashing beats original's simpler path. | [Region overhaul][c-99a79750], [keys/cache refinement][c-f08bb1d1]; `GetRegionHash`, `RegionHashesCache`, `ResourceHandleInfo`. |
| I08 | Divert suitable mapped buffer writes to CPU-owned memory; adopt the completed allocation as cached data on unmap instead of making an additional CPU copy. | When tracking regions on eligible VB/IB/CB mappings, with map-mode constraints. | Eliminates an extra cache-copy operation; upload back to the original mapping remains. Allocation and upload still cost time. Nondivertable writes cannot safely receive the same treatment. | [Adopt instead of memcpy][c-2cb7d4b4]; `TrackAndDivertMap`, `TrackAndDivertUnmap`, `UpdateResourceDataCacheFromMap`. |
| I09 | Stop zeroing the entire diverted memory allocation before the application writes it. | Same eligible map-diversion path. | Removes one whole-allocation zero-fill. This is appropriate only when downstream semantics tolerate unwritten bytes; not a generic permission to read undefined map data. | [Remove zero-fill][c-908f629a]; [current mapped-resource path][s-context]. |
| I10 | Mirror `CopySubresourceRegion` updates between existing CPU snapshots, rather than discarding destination data and forcing a later GPU readback. | Region tracking, supported buffer copies, known source snapshot. | Saves a later staging copy/map on cacheable copies; still performs the CPU-region copy and invalidation. Uncached source falls back to clearing destination snapshot. | [CPU-copy fast path][c-f5a770a8]; `CopySubresourceRegionCache`, `SetDataCacheRegion`. |
| I11 | Defer per-resource hash-table allocation until used; reduce initial global-cache capacity. | Retained lazy structure; current global region cache starts at 1024. | Reduces unused allocation/startup footprint. More growth/rehash may occur for a large active working set. Not equivalent to faster hot-cache hits. | [Lazy local allocation][c-d42d2674], [global initial-size change][c-748521e0]; `RegionHashesCache::Add`, global cache declaration. |
| I12 | Hunting batches visited-VB registration under one global critical section and avoids hash/slot/snapshot work unless relevant buffer selection/cycling requires it. Optional last-seen bookkeeping runs only with an enabled lifetime policy. | Hunting + region hashing; largely irrelevant during normal play. | Reduces lock transitions, selected-slot scanning and unnecessary region queries. Some old WIP changes were superseded; use current `BeforeDraw` and registration routines, not any individual intermediate commit as the final state. | [Batch registration][c-5be2ac54], [conditional hunting work][c-d14437ac]; `BeforeDraw`, `RegisterVisited*NoLock`. |
| I13 | Per-context staging/readback buffers are cached by power-of-two size bucket. `store` and snapshot fallback share them. | Retained; requests create a buffer only on first use of a bucket. | Repeated same/similar-size requests avoid `CreateBuffer` and associated resource setup. GPU→CPU copy and blocking read `Map` remain. Allocated bucket is at least requested size and normally less than twice it, within representable supported sizes. | [Staging-buffer pool][c-7cf7c34d], [snapshot integration][c-4a6062d1]; `GetReadbackBuffer`, `CacheBufferData`, `Store`. |
| I14 | User-addressable custom pools add lazy resource/variable element creation, indexed reuse, FIFO assignment and optional expiration/batch operations. | Only mods declaring/using pools. Current `pool_lazy_initialization=true`; expiration defaults to disabled. | Can avoid author-managed large sets of resources and defer unused creations. This differs from original's existing internal copy-resource `ResourcePool`. Expiration scans once/frame when enabled; frequent expiry can recreate objects and worsen hitches. | [New custom pools][c-3c1029d3], [expiration][c-9fbc28d0], [pool overhaul][c-9e5ab2c9]; `CustomResourcePool`. |
| I15 | Spatial pool index uses quantized position hashes for exact hits and nearby-slot reuse; proxy chains share root pool state instead of duplicating it. | Explicit spatial/proxy pool users. | Exact hashed lookup is expected O(1); nearest-cell fallback scans the pool O(P). Memory/identity reuse is workload-dependent and nearby-cell sharing changes semantics. Proxy cycle protection is correctness, not an FPS optimization. | [Spatial index][c-1cc9c0c6], [proxy chains][c-4f3722ca]; `GetElementIndex`, `SetSourcePool`, `ResolvePool`. |
| I16 | Custom resources can expose lightweight cached-data views; constant-buffer snapshots participate in region/spatial/hash getters. | Only resource commands that need these features. | Reusing CPU data avoids copying/readback for eligible reference/full-copy views. Ownership/lifetime and write invalidation must remain correct. New query expressiveness is not itself a speed gain for an unchanged config. | [Cached-data refactor][c-8d4dcb2f], [views][c-451b9d96], [CB cache][c-046c7bfe]; `ResourceHandleInfo`, custom-resource handle information. |
| I17 | Extend existing static expression evaluation to optional variable/time/frame snapshots and permit expressions during constants/options parsing. Add immutable `locked` variable declarations. | Parse-time consumers explicitly request evaluation. Runtime optimization still treats ordinary variable operands as dynamic. | Moves eligible expression work to load time and permits richer configuration. Original already constant-folded literal trees; the entire optimizer cannot be credited to XXMI. `locked` is assignment validation and does not by itself establish every use is folded to a literal. | [Variable static evaluation][c-76e6edad], [parse-time expressions][c-de03a3c9], [locked vars][c-db64f5b1]; `CommandListOperand::static_evaluate`, `CommandListOperator::optimise`. |
| I18 | Favor runtime speed in DX11 compiler settings; enable multiprocessor compilation. | Retained Release/Zip Release; **already present in original 1.4.9**, incremental only versus 1.3.16. | `/Ot` is compiler policy, not a measured speedup. `/MP` changes build throughput, not runtime. `/O2` and whole-program optimization already existed; current use is not proof of a new optimization. | [Original-project build change][c-87293a7c]; [current project][s-project], [original 1.4.9 project][original-project-new]. |
| I19 | Original post-stable upscaling path removes a Deviare pass-through trampoline in favor of direct calls. | **Already in original 1.4.9**; relevant only where the corresponding upscaling/factory hooks execute. | Removes an indirection, not a blanket improvement to all draw interception. No isolated timings in the inspected history. | [Original-project trampoline removal][c-c0e50daf]. |
| I20 | Thread-safe lazy COM initialization for WIC/Shell image loading. | Retained image-resource loading paths. | Defers COM setup until needed and prevents unsafe initialization races. Primary motivation is reliability/lifecycle; do not convert it into a per-frame speed claim. | [COM initialization][c-11b2a1da]. |

### Optimization proposed upstream but absent from current EDHM

XXMI [hash-driven input-layout cache][c-271e97df] computes a descriptor/signature key, returns an existing wrapper on a cache hit, and retains wrappers in `mInputLayoutCache`. Current EDHM has **no** `FindCachedInputLayout` / `CacheInputLayout` / `mInputLayoutCache` implementation. Current `HackerDevice::CreateInputLayout` calls the real D3D device and attaches a `HackerInputLayout` side-car afterward. The side-car preserves element metadata, layout hash and shader signature for analysis/overrides; it is not device-level creation deduplication. [Current device][s-device], [current side-car][s-layout].

This distinction matters when attributing speed: one cannot list input-layout deduplication as an optimization active in the audited DLL merely because the upstream port notes mention layout caching. Any future deduplication design must preserve genuine `ID3D11InputLayout*` pointers, signatures, descriptor equality/collision handling and correct COM lifetimes. Returning XXMI wrappers is incompatible with the documented Elite invariant.

### Startup deferral is a tradeoff, not removed work

XXMI's [`skip_early_includes_load`][c-aa0bd287] can defer included INI parsing until delayed configuration initialization. That may shorten early startup while shifting work to a later reload and leaving features unavailable initially. Current EDHM defaults it to **false** to load themes and TextureOverrides before first frame. It should not be re-enabled as a free performance optimization. [Current parser][s-ini], [shipped settings][s-config].

## Architectural changes and their consequences

### A. 3D stereo wrapper → 2D mod runtime

The fork retains the D3D11 proxy/hook, device/context wrappers, shader substitution, command interpreter, resource matching, overlay and analysis architecture. It removes the stereo-specific branches and resources rather than replacing the renderer with a new backend. Wine compatibility in local EDHM is host/lifecycle integration; it is not an independent rendering engine. The remaining `nvprofile` code manages driver profiles and does not restore stereo operations. [Stereo strip][c-1921b4fa], [current device/context sources][s-device].

### B. Whole-resource hashes → optional content-region identity service

Original matching mostly resolves a resource hash or fuzzy descriptor, then applies draw filters. The new region system tracks VB/IB bindings and draw offsets/counts so several meshes packed in one GPU buffer can have separate identities. Its pipeline is:

```text
D3D resource create/update/map/copy hooks
  → CPU-owned snapshot and range invalidation
  → (resource, offset, size) fast region-result lookup
  → per-resource (offset, size) region-result lookup
  → hardware CRC32C over the CPU snapshot, or cold staging readback
  → draw-prefiltered TextureOverride matching / hunting / INI getters
```

Caches, data capture, invalidation, object release and matching form one dependency cluster. Cherry-picking just the fast lookup without its update/release hooks risks stale identities. Current EDHM also hardens boundaries, synchronization and page-range invalidation; these later correctness fixes are not improvements invented by the initial XXMI cache implementation. The region feature can cost more than old whole-resource matching because it solves a harder identity problem. [Core cache implementation][s-hash], [update hooks][s-context], [consumer][s-command].

`EncodeFloat30`/resource-identity and spatial getters encode identifiers into the interpreter's float representation. They support the language's identity use cases; they are not automatically equivalent to collision-free full-width identity, nor an independently proven performance gain. [Identity encoding change][c-3894a79f], [current hashing helpers][s-hash].

### C. Internal reuse cache → authored pool state model

Original's internal `ResourcePool` reuses copy-operation destinations. XXMI's `CustomResourcePool` adds a user-facing object model: resource or variable slot elements, a resource template, index policy, optional UID table, expiration metadata, persistence and root-pool proxies. Ring/static indices directly select slots; FIFO maps identifiers; spatial resolves exact or nearby cells. Pool operations and getters are now interpreter features. They introduce API/lifetime complexity and potential memory/CPU costs even when they enable better author-level reuse. [Original internal pool][original-command], [new implementation][s-command], [pool declaration][s-command-header].

### D. Per-operation staging → context-owned readback service

`HackerContext` owns size-bucketed staging buffers, and consumers borrow them rather than repeatedly creating/destroying one. This is a deeper context service shared by `store` and CPU snapshot acquisition. Context teardown owns cleanup. It does not introduce asynchronous GPU readback, double buffering or a worker thread: `CopySubresourceRegion` followed by `Map(D3D11_MAP_READ)` can still stall. [Service introduction][c-7cf7c34d], [consumers][c-4a6062d1], [current context][s-context].

### E. Small INI parser extensions → reusable argument-reader and richer expression API

`CommandArgumentReader` centralizes tokens, variables, targets and argument parsing used by assignments, resource/member directives and presets. Pool operands, member getters, binary/unary operations, frame/draw/dispatch counters, callback/proxy command lists and rewritten `store` broaden the command language. These are architecture/API changes; adding syntax does not establish a parser-throughput or per-frame improvement. Preserve UTF-8 names, hyphen subtraction, whitespace rules, locked-variable rejection and null/missing-resource behavior when adopting later parser revisions. [Argument reader][c-4041d689], [store rewrite][c-246bd0a4], [current command implementation][s-command].

Current `CommandListOperator::evaluate` evaluates both operand subtrees before applying a binary operator. Therefore this audit does **not** credit XXMI with general runtime short-circuit evaluation of expression trees. Command-list conditional skipping and pool-proxy cycle detection are different mechanisms. [Current evaluator][s-command].

### F. Native layout metadata → wrapped layouts upstream → side-cars locally

XXMI first adds a full COM layout wrapper and later deduplication/signature/override management. EDHM adapts this into a private-data side-car owned by the real D3D layout. The game receives genuine runtime pointers; context override commands bind `GetOrigInputLayout()`. The side-car keeps copied semantic strings, descriptors and signature bytes. This is a significant compatibility and ownership change, with a possible extra private-data lookup, not a measured speed optimization. [Wrapper introduction][c-3f79f697], [upstream deduplication][c-271e97df], [VB-layout commands][c-cde6bab4], [current side-car][s-layout].

### G. Matching semantics changed upstream, restored locally

XXMI [allows fuzzy matches even after exact hash matches][c-e3b66a44]. Current EDHM returns after a successful hash match unless `fuzzy_match_alongside_hash=1`, restoring classic priority/filter behavior and incidentally avoiding fuzzy scans for those hits. This is first a correctness/configuration contract, not a safe universal optimization for other XXMI mods. The region-prefilter branch has its own matching sequence; do not assume the ordinary matcher's early-return statement covers every region-enabled consumer. [Current matcher][s-hash], `ResourceCopyTarget::FindTextureOverrides` in [command-list consumers][s-command].

### H. Build graph/toolchain changes

The local repository's removal of DX9/injector/compiler proxy/platform projects reduces source/build/package scope. It does not prove the loaded D3D11 DLL consumes less frame time. VS2022/v143/C++17 modernization exists in original 1.4.9 as well; compiler selection, runtime optimizer flags and parallel compilation must be distinguished. The main local audit covers EDHM's slim graph and quality tooling. [Current project][s-project], [original 1.4.9 project][original-project-new].

## Concrete findings and measurement priorities

### F01: Duplicate disassembly on the pattern path

At current `HackerContext.cpp` lines 684–693, two consecutive `BinaryToAsmText` calls receive identical bytecode/settings. Each is followed by an empty-result check; the second assignment overwrites the first successful text. The duplicate is already present in XXMI `v0.9.8`, introduced when [the no-pattern shortcut][c-6ebd2733] inserted another disassembly call before the pre-existing one. Local formatting is not its cause. [Current lines][s-duplicate], [XXMI v0.9.8 path][tag-context].

Recommended experiment: on a disposable Release variant remove one successful duplicate call, retain one call and error handling, then measure cold ShaderRegex pattern-rule analysis time and first-use frame hitches. Warm caches/no-pattern rules are controls and should not be expected to improve. Correctness gate: identical assembled replacement bytecode and rule linking for fixed shader fixtures. **Known source-level effect:** two calls become one for affected successful misses. **Measured ms/frame/FPS effect:** unavailable here. Treat this as a high-priority small candidate, not authority to claim a 2× runtime improvement.

### F02: Region hashing can hide cold GPU synchronization

`GetRegionHash` explicitly falls back to copying a GPU buffer into staging and mapping it when no snapshot is present. The fast cache paths and pooled staging objects do not remove that dependency. Profile cache hit ratios, fallback readbacks and `Map` wait duration separately. Keep `track_region_hashes=0` in normal EDHM until a mod actually needs region identity. [Current fallback][s-hash], [shipped default][s-config].

### F03: Custom-pool misses and expiration are not constant-time

Spatial fallback loops over `index_table`, and `ExpireElements` can scan it once per frame. On an exact spatial hit the expected lookup cost is small, but a large moving set or aggressive timeout changes that profile. Benchmark exact hit, near-cell reuse, total miss and expiration separately before choosing an index policy. A ring/FIFO pool may be preferable when the domain supplies stable identifiers. [Current `CustomResourcePool`][s-command].

### F04: Optimization gains must be evaluated only where active

For ordinary EDHM play, prioritize quiet logging/Present work from the local audit and the actual script workload. No-pattern ShaderRegex, `TIME` caching, region caches, pools and hunting each have different triggers. A single aggregate FPS comparison cannot assign their individual contributions, and subtracting two different releases mixes correctness, config, compiler and feature changes.

## Separate profiling experiments required

The table names the outcome needed for each catalog item; it does not claim those experiments have run. Use fixed source revisions, one change at a time or an ablation from a fixed compatible base, identical Release compiler/settings/config/assets, alternating trial order, warmup, repeated captures and raw retained samples. Report confidence intervals and frame-time distributions; do not add percentage gains from interacting caches.

| IDs | Isolated workload / outcome | Essential controls |
|---|---|---|
| I01 | CPU time in stereo setup, SRV binding and draw hooks; wrapper overhead per draw | Compare stereo-disabled original and fork; enabled stereo has different functionality. |
| I02 | Cold startup and config reload wall/CPU time; getter hit/miss cost | Same INI tree; independent missing-section, missing-key and present-key cases; UTF-8 conversion separate. |
| I03 | Cost per `TIME` evaluation and full scripted frame with known evaluation count | Same clock semantics where feasible; no timing work folded away by compiler. |
| I04 / F01 | Cold deferred-analysis duration and first-use frame-time spike per shader | Patternless, matching Patterns, nonmatching model and warm cache separately. |
| I05–I07 | Candidate count, CRC bytes/calls, L2/L3 hit/miss, CPU time and map stalls | Same correct matches; count misses and selected draw sizes, hot/cold snapshots, deferred contexts. |
| I08–I10 | Map/Unmap and copy-hook CPU time, memory bytes copied/zeroed and subsequent readback count | Identical buffer data/map modes and valid updates, rather than synthetic unsupported API sequences. |
| I11 | Peak/steady working set, allocations, growth and startup time | Small and large active region working sets; no assertion that smaller capacity improves hot-path speed. |
| I12 | Hunting CPU time and frame-time p95/p99 | Same selected buffer/slot and lifetime mode; normal play as a zero/low-activation control. |
| I13 | Staging `CreateBuffer` count, allocation memory and readback/map time | First bucket request, repeated bucket hit, varying buckets; preserve GPU synchronization work. |
| I14–I16 | Pool reuse, object creates, bytes resident, CPU time, eviction/recreation | Equivalent authored behavior; ring/FIFO/spatial hits/misses and timeout behavior separate. |
| I17 | Parse/reload time and runtime command CPU time | Expressions with literal values vs runtime variables; identical final values and mutable behavior. |
| I18 | DLL runtime performance for compiler-policy change; build wall time for `/MP` | Original 1.4.9 already has settings; compiler build, CPU concurrency and incremental/clean builds pinned. |
| I19 | Upscaling hook CPU time | Same upscaling path and behavior; irrelevant if path never executes. |
| I20 | Image-load startup/first-use time and reliability | Same image codec/resources, same thread/apartment assumptions; do not infer steady-frame gain. |

For every feature, report **“not measured”** if its outcome is unavailable. A path disabled in the tested configuration is **“inactive in this workload”**, not universally zero-cost or ineffective.

## Evidence and reproduction

The inventory was built from `git log --no-merges 1.3.16..v0.9.8 -- DirectX11`, the corresponding file diffs, a second comparison to original `1.4.9`, and direct current-source checks. Individual commit subjects were used as discovery aids, not performance evidence. Current-source links are pinned so line references remain stable; reference definitions below pin complete commit hashes.

Useful read-only commands:

```bash
git diff -w 1.3.16 v0.9.8 -- DirectX11/
git diff -w 1.4.9 v0.9.8 -- DirectX11/
git diff -w v0.9.8 34d92f27 -- DirectX11/
git show 1.3.16:DirectX11/CommandList.cpp
git show 1.4.9:DirectX11/DirectX11.vcxproj
git blame -L 683,693 34d92f27 -- DirectX11/HackerContext.cpp
```

The baseline branches diverge; an absence from `git log A..B` is not sufficient evidence of an absent equivalent implementation. This appendix checks function behavior before classifying an optimization as present, adapted or absent.

[s-context]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/HackerContext.cpp
[s-device]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/HackerDevice.cpp#L1809
[s-layout]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/HackerInputLayout.cpp
[s-ini]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/IniHandler.cpp
[s-dxgi]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/HackerDXGI.cpp#L195
[s-hash]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/ResourceHash.cpp
[s-command]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/CommandList.cpp
[s-command-header]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/CommandList.h#L647
[s-regex]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/ShaderRegex.cpp
[s-project]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/DirectX11.vcxproj
[s-config]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/Dependencies/d3dx.ini
[s-duplicate]: https://github.com/Fenris159/EDHM_2DMigoto/blob/34d92f27f588bbc3f72320c0fa17b674638698c6/DirectX11/HackerContext.cpp#L683
[original-command]: https://github.com/bo3b/3Dmigoto/blob/cbc4993ca67b6d639e06c578d4e3dc8b3780243f/DirectX11/CommandList.cpp
[original-hash]: https://github.com/bo3b/3Dmigoto/blob/cbc4993ca67b6d639e06c578d4e3dc8b3780243f/DirectX11/ResourceHash.cpp
[original-project-new]: https://github.com/bo3b/3Dmigoto/blob/4ce5f2f72b2777223d2e809dcdafec514ac98295/DirectX11/DirectX11.vcxproj
[tag-context]: https://github.com/SpectrumQT/XXMI-Libs-Package/blob/f8e7118a1220e43546e546efa31892b8304358df/DirectX11/HackerContext.cpp#L601
[c-1921b4fa]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/1921b4fa6c99267c2f82d3d259a3074cc92f81a5
[c-beb7f481]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/beb7f48192bf999af00cfa9c173e253c8a34c8af
[c-46b9540c]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/46b9540c74c28cf43ffb1feeaee1b49d7de98035
[c-6ebd2733]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/6ebd27333520fa3b281197c9a8137dd3a5de6102
[c-fcf65acd]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/fcf65acd0ce11ff200abd083a3baf9d4e6e10a81
[c-4eba8754]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/4eba875461795476113b8876f07d25aa2b5e7fa6
[c-99a79750]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/99a797503280224f602a60c7594a44023061fbe5
[c-f08bb1d1]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/f08bb1d1716f45eec956f837c96b6542c4a9f316
[c-2cb7d4b4]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/2cb7d4b4f8308d5bbc0e49187988a9f9a340586d
[c-908f629a]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/908f629a9ff39bba8808c3a8619ecb023d0cf427
[c-f5a770a8]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/f5a770a84ccdecb4a1f2a8806c8230edc6efe6ee
[c-d42d2674]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/d42d26742fd49b87ae0559db0fa9633322f55518
[c-748521e0]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/748521e076d731509eae3a6ac1d432396991c638
[c-5be2ac54]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/5be2ac54fe025fe01f1718a2fe3dc435c17e9835
[c-d14437ac]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/d14437acdf619d07a817ed4c0ebb936c68306925
[c-7cf7c34d]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/7cf7c34d47cdc6016f016c092721d81b7d8fd9fd
[c-4a6062d1]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/4a6062d14e00b75b06b44f6e670f31b61e353f7b
[c-3c1029d3]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/3c1029d354133babbddbb9d1b00bdc4fa5033f56
[c-9fbc28d0]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/9fbc28d0398c3871130fae15a744a3561d7c874e
[c-9e5ab2c9]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/9e5ab2c9c24efb37e4874a39cb8b288003966dbf
[c-1cc9c0c6]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/1cc9c0c6c30aa3f066b22096f642993a37ee3f1a
[c-4f3722ca]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/4f3722cafe71f002e48a5a621bdff3b31290ef1d
[c-8d4dcb2f]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/8d4dcb2fa1fa405964860d75d123ba3cd65557e1
[c-451b9d96]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/451b9d964cc94526ebc3320bb860a249f4d3fe94
[c-046c7bfe]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/046c7bfe73998bad0b605e56466bdaa55e7bff3a
[c-76e6edad]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/76e6edad55fa319601e3628b84ad126a77c26ac5
[c-de03a3c9]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/de03a3c938eb6140730e44239917ddf0ef9b4bba
[c-db64f5b1]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/db64f5b188c59be830a2414c07f897fdc296ce18
[c-87293a7c]: https://github.com/bo3b/3Dmigoto/commit/87293a7c9a3c2be2e3521218199f9f6e059f6edf
[c-c0e50daf]: https://github.com/bo3b/3Dmigoto/commit/c0e50dafac74e77f0755dfb9935599f86a5eba4d
[c-11b2a1da]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/11b2a1da263a401e7babc5275acc0228391ae75f
[c-271e97df]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/271e97df87e8b097936744eecb39e4c6d9d10e8c
[c-aa0bd287]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/aa0bd28790070cda1960ff7b43fa96f6eb3e5ae8
[c-3894a79f]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/3894a79fde418239edb8a83d6ce0fd51782105ee
[c-4041d689]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/4041d689404862330365838bad4fe9a288ef89c7
[c-246bd0a4]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/246bd0a4226bb78a7506b422dabc712c34235075
[c-3f79f697]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/3f79f697c6fa36c8eca6c6a44252c30ac777dc6e
[c-cde6bab4]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/cde6bab4a5b730e1a50552a66c62d291f9133735
[c-e3b66a44]: https://github.com/SpectrumQT/XXMI-Libs-Package/commit/e3b66a44cf092aa94d8565bc98bb336f70057aa5
