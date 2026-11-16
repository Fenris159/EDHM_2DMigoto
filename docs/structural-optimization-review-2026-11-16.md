# Structural optimization opportunities

**Baseline:** EDHM `develop` at `4ae925945879a897309992f58db6cc752f22289d`,
16 November 2026. Source review only; runtime, installed files and tests remain
unchanged. Proposed speed benefits are unmeasured.

**Architecture refinement:** the [main plan][main-track] now separates deepening
candidates from the empty-list experiment, splits region views from pools, and
defers interface selection. Its ordering and gates supersede the preliminary
sketches below.

[main-track]: edo-next-readiness-2026-11-16.md#local-structural-track-after-ranks-12

The strongest structural opportunity is to give a few modules clear ownership
of decisions and their validity period. Compute static facts during load,
resource facts when their identity changes, and draw-dependent facts at the
draw. Existing optimizations already do some of this; extend them selectively.
Splitting large files alone would improve navigation without establishing a
runtime benefit.

## Recommended order

### 1. One module for parameter publication

Values live in `G->iniParams`, but command assignments, transitions and
initialization each participate in publishing them. Upload and failure policy
are duplicated across [command flushing][flush], [transitions][transition] and
[initialization][init]. The transition path's unchecked Map is an example of
those policies drifting.

A small interface should own assignments, pending publication, success/retry
state and the device resource's uploaded generation. Keep existing legal upload
points; draws may need different values within one frame. Fix the failure path
first, then migrate existing callers incrementally. Do not combine this with a
new backend or a different GPU representation.

**Possible benefit:** reliably skip proven redundant uploads and handle retries
once. **Proof needed:** unchanged rows avoid unnecessary publication where
semantically legal; two draws with different rows retain both outputs; failed
final-transition uploads retry the final value. Measure Map calls, copied bytes
and CPU cost. A centralized module alone is not a speed result.

### 2. Separate read-only queries from commands with effects

The installed root INI reads `ps-t0` in an assignment and immediately reads it
again in a condition ([local example](</home/drew/.local/share/Steam/steamapps/common/Elite Dangerous/Products/elite-dangerous-odyssey-64/d3dx.ini:731>)).
Each [numeric filter evaluation][query] obtains resource/view information and
builds matching results through [target lookup][lookup].

A query module could retain results within a proven read-only command segment.
Its interface must state dependencies and invalidation: bindings, resource/hash
changes, draw information, configuration generation and pool state. Unknown or
mutating commands end that reuse period. `checktextureoverride` executes command
lists; some resource getters can materialize or expire resources, so neither
operation can automatically be treated as a pure query.

**Possible benefit:** fewer COM queries, temporary match allocations and repeated
predicate evaluations in the actual EDHM path. **Proof needed:** repeated pure
reads agree, while rebinding or an intervening command immediately changes the
result. Preserve negative-zero/unmatched/index semantics. Compare query counts
and off-game CPU cost; do not cache by frame alone.

### 3. Give the executor an inexpensive empty-plan path

[RunCommandListComplete][execute] prepares execution state and device/context
pointers before [_RunCommandList][run] resolves the list and discovers it is
empty. A conservative check after the existing configuration lease could avoid
that setup for an empty resolved list.

Keep proxy resolution per invocation and handle runtime-populated lists
conservatively. Nested configuration leases already reuse their outer lease;
there is no basis here for removing synchronization. The existing
[optimizer][optimize] already folds expressions and eliminates no-ops, so a
bytecode-interpreter rewrite is a larger, unproved next step.

**Possible benefit:** less setup for empty pre/post lists. **Proof needed:**
empty lists preserve behavior/profiling, and a proxy or runtime-populated list
that later gains commands still executes them. Measure empty and nonempty
workloads separately.

### 4. One normalized resource/view request, with demand-driven metadata

The resource-copy module combines target resolution, metadata preview, copy
planning, view validation and binding. Ordinary view reuse already exists.
However, [region references discard cached views][region] each invocation;
an identical region request can therefore repeat view creation. Separately,
[pool metadata previews][pool] clone the pool's maps and elements, even when a
query touches one slot.

A normalized, owned request could carry the resolved resource, effective kind,
format, offset/extent and descriptor through planning and execution once. A
later sparse metadata transaction could retain only touched state, preserving
FIFO/spatial selection, expiration, aliases and destination evaluation rules.

**Possible benefit:** fewer view creations and less whole-pool copying.
**Relevance limit:** the inspected nine-file EDHM configuration contains no
Pool sections or Region expressions. These are conditional opportunities for
advanced workloads, not a demonstrated gain for the current pack.
**Proof needed:** identical region requests reuse views; changed range/format/
kind invalidates them; metadata queries leave live pool state untouched. Compare
small and large pools and all existing alias/expiration/destination outcomes.

## Structural limits

These modules should hide complexity behind small interfaces. Their value is
avoiding repeated work and concentrating ownership/invalidation rules; adding
more forwarding layers would not achieve that. Keep existing descriptor checks,
configuration lifetimes, synchronous `store` results and EDUP presentation order.

I would start with parameter publication and bounded query reuse, then the
empty-plan path. Defer interpreter rewrites, broad removal of globals, a renderer
abstraction and pool redesign until a relevant workload demonstrates the cost.
New regression cases require confirmation before implementation; none were
written or run in this review.

[flush]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/CommandList.cpp#L447
[transition]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/Override.cpp#L483
[init]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/HackerContext.cpp#L3350
[query]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/CommandList.cpp#L2970
[lookup]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/CommandList.cpp#L10044
[execute]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/CommandList.cpp#L468
[run]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/CommandList.cpp#L177
[optimize]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/CommandList.cpp#L521
[region]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/CommandList.cpp#L12342
[pool]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/CommandList.cpp#L7243
