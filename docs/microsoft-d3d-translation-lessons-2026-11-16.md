# Microsoft translation-layer lessons for EDHM and edo-next

## Status and recommendation

- **Inspected:** 16 November 2026, source and official API documentation only.
- **EDHM baseline:** `4ae925945879a897309992f58db6cc752f22289d` (`develop`,
  runtime matching 0.1.8). Existing local changes were preserved.
- **D3D11On12:** `c6b5a961ee5e0761136fc2091b5e57f7bd9235f2`.
- **D3D12TranslationLayer:** `4b9c8f8a49c9b68df19be817d56f123d65704fd2`.
  These are public source snapshots, not identification of an installed Windows DLL.
- **Conclusion:** use these projects as correctness and performance-design
  references. Keep today's D3D11 route; do not add a translation backend or
  import the full libraries merely to prepare an edo-next plugin.
- **Useful now:** investigate the transition-upload failure path, inventory
  uploads/readbacks and carry precise ownership/cache requirements into the
  [edo-next preparation plan](edo-next-readiness-2026-11-16.md).
- **Limits:** no builds, new tests, benchmarks, live-game/FPS monitoring,
  installations or runtime changes. All improvements below are candidates;
  no performance percentage or new compatibility certification is claimed.

## What these projects would add

**D3D11On12 is a driver mapping layer, not a replacement `d3d11.dll`.** Its
export is `OpenAdapter_D3D11On12`; the Windows D3D11 runtime feeds it driver
commands. It uses D3D12TranslationLayer to implement the translation. The public
`D3D11On12CreateDevice` API requires an existing D3D12 device; coordinated
interop uses an explicitly supplied graphics/direct queue (the API's queue
array parameter is optional)
([source architecture][on12-readme], [export][on12-export], [API][create-api]).

**D3D12TranslationLayer is the larger implementation library.** It manages
bindings, resource states, allocation, renaming, lifetimes, batching and PSOs.
Those are mostly renderer responsibilities, which our slim D3D11 proxy delegates
to the backend today. Its own roadmap describes `ImmediateContext` as monolithic
and difficult to consume piecemeal ([library design][tl-readme]).

| Route | Implication for our goals |
| --- | --- |
| Current game D3D11 → optional EDUP chain → EDHM → native D3D11 or DXVK | Preserve this route and its existing shader/configuration behavior. |
| Whole-game D3D11 → Windows D3D11 runtime → On12/TL → D3D12 | A different backend experiment, with additional translation, memory and synchronization responsibilities. Not a configuration shortcut to edo-next. |
| Proposed edo-next native D3D12 renderer + separate service-based plugin | Apply the lessons in the host/SDK contract; the existing EDHM DLL does not become a native plugin. |

Microsoft's published guidance warns of CPU and memory overhead and recommends
On12 chiefly for simpler/2D interop. That is a reason to require measurement,
not proof that every workload is slower ([guidance][overview]). The source
README also advises applications against shipping a private replacement for
the Windows On12 component ([distribution boundary][on12-distribution]).

Our DLL **already forwards `D3D11On12CreateDevice`** and retains an older overlay
interop path ([forwarder][edhm-on12], [overlay][edhm-overlay]). That path does
not provide D3D12 shader replacement. The 0.1.7/0.1.8 repair deliberately passes
EDUP's D3D12 helper swap chains through unwrapped, preserving native interfaces
([pass-through][edhm-pass]). Re-enabling the warning-overlay wrapping for them
would undo that compatibility boundary.

## Ranked work worth taking from the study

### 1. Check transition-upload failures, then measure avoidable uploads — local candidate

`Override.cpp::UpdateIniParams` maps the dynamic parameter texture, then copies
and unmaps without checking the returned HRESULT. In contrast,
`CommandListFlushState` stops on a failed Map ([transition helper][edhm-transition],
[checked path][edhm-flush]). This is a **source-found, untested failure-path
hazard**, not a reproduced crash or an explanation of issue #52.

The smallest follow-up is a guarded upload that preserves the latest CPU values
and retries safely when appropriate, with explicit device-loss behavior. Then
measure unchanged-value uploads before considering their removal. The current
command path already marks changed rows dirty, so "add dirty tracking" would
overstate what is new ([assignment][edhm-assignment]).

**Benefit/risk:** first improve failure handling; any CPU benefit depends on
measured redundant work. Small implementation scope, but retry/order semantics
must be protected. **Proposed proof:** a failed upload neither writes unmapped
memory nor discards pending values; a later successful upload exposes the latest
literal values. Include failure on the final transition value: completed entries
are erased before upload, so a guard alone would not guarantee that value is
retried ([completion order][edhm-transition-order]). Unchanged settings preserve
visible output and timing/order.
No new test was written here.

### 2. Preserve semantic upload/readback boundaries — useful now as an inventory

TL explains why dynamic memory needs renaming: the GPU may still be reading the
previous contents. It also synchronizes Map against resource use; even a
non-blocking request can cause submission work ([renaming][tl-renaming],
[synchronization][tl-map]). Pooling does not remove that dependency.

EDHM already pools staging buffers by size and bounds `store` copies, but `store`
then maps synchronously and returns a value to the current command sequence
([pool][edhm-readback], [store][edhm-store]). Do not silently delay its result,
zero it on failure, or coalesce all current INI uploads to one Present. Draws
inside a frame may intentionally consume different values.

**Benefit/scope:** extend the passive inventory with readback-dependent rules,
upload points, copied byte counts and retained allocation sizes. Prefer existing
profiling counters/off-game probes over new per-draw logging. **Risk:** low for
inventory; high if changing synchronous semantics. **Proposed proof:** two draws
requiring different parameter snapshots retain both outputs; read failures keep
the previous variable value. Compare CPU submission cost, Map waits, allocations
and bytes separately, without an FPS claim.

### 3. Make PA4/PA4b caches depend on effective layout and resource identity — host/SDK work

TL's binding cache considers shader-declared slots, typed nulls and root-signature
size changes. It reasserts bindings when layouts change. Resource renaming changes
a uniqueness value and invalidates views ([dirty slots][tl-dirty],
[root changes][tl-root], [view generation][tl-view]). These are particularly
useful references for proposed shader pairing and bound-slot texture tags.

**Smallest host requirement:** distinguish original shader identity from the
effective replacement/transform/layout version. Include pairing/pipeline rules,
constant-block rebasing and tag-buffer layout in variant invalidation. Update
slot tags when the host constructs changed bindings; do not hash texture content
or invoke plugin code on every draw. Use resource/view generations rather than
a recycled address as the entire identity.

**Benefit/risk:** fewer unnecessary rebuilds with correct panel selection;
medium/high host integration risk, dependent on PA4/PA4b. **Proposed proof:**
alternate tagged, untagged and null resources under one PSO; change root layout
and reuse a descriptor address; require the expected output each time. Test
required/excluded VS pairs independently. Existing EDHM view caches already
check resource and effective descriptors ([current cache][edhm-views]); this
study does not establish that replacing them would help.

### 4. Use copied snapshots and fence-retired storage — host/SDK work

TL copies input into owned upload/batch storage and defers GPU resource retirement
according to last use. It distinguishes objects needing submission from resources
needing completion; its bounded pools may wait when full ([owned upload][tl-upload],
[retirement][tl-retirement], [bounded pool][tl-pool]).

For edo-next, the plugin should publish copied row snapshots; the host should
own in-flight constant/tag slices, descriptors, fences and memory budgets. A
successful CPU copy does not make the GPU's previous slice reusable. Conversely,
EDHM's retired CPU registry protects late COM callbacks during shutdown; it is
not a GPU fence mechanism and must not be replaced by one
([registry retirement][edhm-retirement]).

**Benefit/risk:** avoid stale colours, tags and use-after-free while bounding
memory; high ownership/synchronization risk. **Proposed proof:** submit A and B
before A completes and observe A/B respectively; delay fences and prevent early
reuse; update/reload/free caller data after registration without corrupting the
registered snapshot. Keep a defined capacity/fallback policy.

### 5. Prove submission and non-blocking bake behavior — host work

The latest inspected TL change is instructive: after copying/resolving into the
back buffer, a final barrier can land in a new command list. The fix counts that
barrier as work so Flush submits it before Present ([fix][tl-present]). It is
not evidence of that defect in our DLL, which does not own this scheduler.

Worker compilation also has limits: TL's `GetForUse` waits for pending PSO work.
It therefore does **not** implement edo-next's proposed original-until-ready
guarantee merely by having workers ([wait][tl-pso-wait]).

**Smallest host requirements:** barrier-only lists count as work; required queue
dependencies are submitted before presentation; pending plugin bakes use original
output without blocking the render path. **Benefit/risk:** avoid device removal
and bake stalls; high host correctness risk. **Proposed proof:** force a copy or
MSAA resolve to submit first, leaving only a final barrier; validate correct
submission/output. Deliberately delay a bake and require original output followed
by a safe switch. Check necessary cross-queue waits without adding unconditional
flushes. Do not copy TL's numeric submission thresholds into EDHM.

### 6. Use capability-based compatibility and memory tests — later experiment

NVIDIA, AMD and Intel names are insufficient to select a path. Check OS/API
availability, the actual adapter/device/queue, feature support and resource
formats. Distinguish discrete, UMA and cache-coherent UMA using capabilities
([architecture query][architecture-api]). TL queries those properties, but its
README still lists special discrete-copy/UMA upload optimization as roadmap
work; it is not a finished fast path to transplant ([roadmap][tl-roadmap]).

**Benefit/scope:** a later optional off-game test lane can compare the existing
backend and system-provided On12, including resize, teardown, typed/structured
buffers, shader outputs and failure handling. It must not change the default
backend. Record driver/OS, CPU cost, copies, waits, memory and literal outputs;
keep cold compilation separate from warm execution. **Risk:** medium/high;
needs available hardware and separately approved cases. No new backend support
is established by source inspection or WARP alone.

| Environment | What this study establishes |
| --- | --- |
| Windows NVIDIA/AMD/Intel | Vendor-neutral API eligibility subject to features and drivers; no new device/vendor result. |
| Windows WARP | Useful for future correctness checks; not evidence of hardware speed or vendor feature support. |
| Proton with DXVK | Its D3D11-to-Vulkan route is different from Windows On12. Preserve the existing route; Windows source does not certify it. |
| Proton with a D3D12 host | vkd3d-proton supplies D3D12 over Vulkan and has its own driver requirements. Validate this route separately; no inherited Windows result. |

Primary platform sources: [DXVK][dxvk] and [vkd3d-proton][vkd3d].
DXVK also implements its own On12 entry point through vkd3d-proton interop;
On12 is not categorically absent on Proton. The inspected DXVK revision exposes
the basic wrapped-resource/Device1 contract, which does not establish Device2
unwrap/return support. Probe the installed interfaces before selecting any
interop path ([entry point][dxvk-on12], [interface][dxvk-on12-interface]).

## A narrow interop option, not a shortcut to full compatibility

On12 can be useful for a small D3D11/D2D overlay on a D3D12 surface controlled
by the host. The boundary needs explicit acquire/release states and submission
ordering. Unwrap/return is another contract: checkout inserts dependencies,
return supplies completion fences, and unwrapping does not itself flush
([unwrap][unwrap-api], [return][return-api]). Keep acquire/release and
unwrap/return ownership states distinct; a wrapped resource can also be checked
out, but must be returned before further On12 access or presentation. Add waits
only at established ownership boundaries.

Such an overlay could later reuse drawing code, but it requires a host service
exposing suitable resources/queues. The proposed edo-next v1 does not provide
those D3D objects. It would not restore scene detectors, texture replacements,
the INI interpreter or a native shader plugin. Prefer the proposed host toast
for a simple warning; defer any overlay bridge until its scope is justified.

Preserve original shader identity above translation: On12 can convert incoming
DXBC to DXIL and cache it by an embedded digest ([conversion][on12-shader]).
That incoming code may already contain EDHM's replacement. Neither converted
bytes nor the driver's "original" are automatically the game's original legacy
FNV key. The existing offline inventory and SDK hash-proof requirements stand.

The immediate next implementation candidate is the guarded transition upload,
with its behavior agreed before tests are added. The largest benefits from the
Microsoft projects belong in future host design and validation. No evidence
from this study warrants a new dependency or forced translation in today's DLL.

[on12-readme]: https://github.com/microsoft/D3D11On12/blob/c6b5a961ee5e0761136fc2091b5e57f7bd9235f2/README.md#L3
[on12-export]: https://github.com/microsoft/D3D11On12/blob/c6b5a961ee5e0761136fc2091b5e57f7bd9235f2/src/d3d11on12.def#L2
[on12-distribution]: https://github.com/microsoft/D3D11On12/blob/c6b5a961ee5e0761136fc2091b5e57f7bd9235f2/README.md#L51
[on12-shader]: https://github.com/microsoft/D3D11On12/blob/c6b5a961ee5e0761136fc2091b5e57f7bd9235f2/src/shader.cpp#L272
[tl-readme]: https://github.com/microsoft/D3D12TranslationLayer/blob/4b9c8f8a49c9b68df19be817d56f123d65704fd2/README.md#L30
[tl-renaming]: https://github.com/microsoft/D3D12TranslationLayer/blob/4b9c8f8a49c9b68df19be817d56f123d65704fd2/src/ImmediateContext.cpp#L5308
[tl-map]: https://github.com/microsoft/D3D12TranslationLayer/blob/4b9c8f8a49c9b68df19be817d56f123d65704fd2/src/ImmediateContext.cpp#L4553
[tl-dirty]: https://github.com/microsoft/D3D12TranslationLayer/blob/4b9c8f8a49c9b68df19be817d56f123d65704fd2/include/ImmediateContext.inl#L242
[tl-root]: https://github.com/microsoft/D3D12TranslationLayer/blob/4b9c8f8a49c9b68df19be817d56f123d65704fd2/include/ImmediateContext.inl#L517
[tl-view]: https://github.com/microsoft/D3D12TranslationLayer/blob/4b9c8f8a49c9b68df19be817d56f123d65704fd2/include/View.inl#L119
[tl-upload]: https://github.com/microsoft/D3D12TranslationLayer/blob/4b9c8f8a49c9b68df19be817d56f123d65704fd2/src/BatchedContext.cpp#L1069
[tl-retirement]: https://github.com/microsoft/D3D12TranslationLayer/blob/4b9c8f8a49c9b68df19be817d56f123d65704fd2/src/ImmediateContext.cpp#L5453
[tl-pool]: https://github.com/microsoft/D3D12TranslationLayer/blob/4b9c8f8a49c9b68df19be817d56f123d65704fd2/include/ImmediateContext.hpp#L113
[tl-present]: https://github.com/microsoft/D3D12TranslationLayer/blob/4b9c8f8a49c9b68df19be817d56f123d65704fd2/src/SwapChainHelper.cpp#L37
[tl-pso-wait]: https://github.com/microsoft/D3D12TranslationLayer/blob/4b9c8f8a49c9b68df19be817d56f123d65704fd2/include/PipelineState.hpp#L121
[tl-roadmap]: https://github.com/microsoft/D3D12TranslationLayer/blob/4b9c8f8a49c9b68df19be817d56f123d65704fd2/README.md#L59
[edhm-on12]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/D3D11Wrapper.cpp#L397
[edhm-overlay]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/Overlay.cpp#L393
[edhm-pass]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/HookedDXGI.cpp#L586
[edhm-transition]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/Override.cpp#L483
[edhm-transition-order]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/Override.cpp#L707
[edhm-flush]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/CommandList.cpp#L447
[edhm-assignment]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/CommandList.cpp#L5484
[edhm-readback]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/HackerContext.cpp#L847
[edhm-store]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/CommandList.cpp#L1782
[edhm-views]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/CommandList.cpp#L12362
[edhm-retirement]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/ResourceHash.cpp#L1272
[overview]: https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-11-on-12
[create-api]: https://learn.microsoft.com/en-us/windows/win32/api/d3d11on12/nf-d3d11on12-d3d11on12createdevice
[unwrap-api]: https://learn.microsoft.com/en-us/windows/win32/api/d3d11on12/nf-d3d11on12-id3d11on12device2-unwrapunderlyingresource
[return-api]: https://learn.microsoft.com/en-us/windows/win32/api/d3d11on12/nf-d3d11on12-id3d11on12device2-returnunderlyingresource
[architecture-api]: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_architecture1
[dxvk]: https://github.com/doitsujin/dxvk/blob/1f1dfc65fc59e7f3b81766e0536d0c9d4639f1a6/README.md
[vkd3d]: https://github.com/HansKristian-Work/vkd3d-proton/blob/f2963ee23c2a392d72c1e383128129b92e53df25/README.md
[dxvk-on12]: https://github.com/doitsujin/dxvk/blob/1f1dfc65fc59e7f3b81766e0536d0c9d4639f1a6/src/d3d11/d3d11_main.cpp#L280
[dxvk-on12-interface]: https://github.com/doitsujin/dxvk/blob/1f1dfc65fc59e7f3b81766e0536d0c9d4639f1a6/src/d3d11/d3d11_on_12.h#L40
