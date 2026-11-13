# Compatibility and resource boundaries: develop versus original 3Dmigoto

**Researched:** 2026-11-13. This appendix contains primary-source and source-code
findings. Executed results, exact DLL hashes and backend identities belong to
the accompanying performance report; this appendix adds no hardware or game
test claims. FPS and live-game monitoring are outside the comparison.

## Source identities and evidence levels

The source comparison uses EDHM_2DMigoto `develop` at
`5c74823ff0d21c662e2d659d04afc6624d245509` and original bo3b/3Dmigoto **1.4.9** at
`4ce5f2f72b2777223d2e809dcdafec514ac98295`. Original **1.3.16**,
`cbc4993ca67b6d639e06c578d4e3dc8b3780243f`, is historical stable context, not a
substitute for the newer original source in this comparison. A shipped-DLL
experiment must identify its release asset and hash separately from these source
references.

- **Documented API support** describes the platform's contracts and prerequisites.
- **Source-established behavior** identifies a path present or absent in the
  pinned sources; it is not a claim that every driver executes it successfully.
- **Executed support** requires a successful run on the named backend. WARP,
  an AMD Vulkan driver, and a physical NVIDIA Windows driver are different cells.

Microsoft defines D3D feature levels as functionality contracts rather than
performance ratings. Device creation and format-support queries, not a vendor
name alone, determine whether the required operations are available.
[Feature levels](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-intro),
[CheckFormatSupport](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-checkformatsupport).

## Vendor and platform matrix

Every row below is an eligibility/source assessment. A measured pass must come
from the main report's matching backend entry; a blank or absent measurement is
not a compatibility certification.

| Technology | Original 3Dmigoto 1.4.9 | EDHM_2DMigoto develop | Boundary of the claim |
| --- | --- | --- | --- |
| NVIDIA + native Windows D3D11 | D3D11 wrapper plus NVIDIA-specific initialization, driver-profile and stereo paths | D3D11 wrapper with that NVAPI-linked control path removed | Requires the selected D3D feature level and formats; no conclusion about every NVIDIA GPU/driver |
| AMD + native Windows D3D11 | D3D11 core exists; NVIDIA-specific initialization is also present | Standard D3D11 interface path without the linked NVAPI startup/profile dependency | API eligibility, not an executed Radeon compatibility pass |
| Intel + native Windows D3D11 | Same distinction between the D3D11 core and the NVIDIA-specific path | Same vendor-neutral D3D11 interface path | API eligibility, not an executed Arc/integrated-GPU compatibility pass |
| NVIDIA + Linux/Proton/DXVK | Legacy chaining/configuration needs to be accounted for | Wine-aware defaults and prefix/system D3D11 chaining are explicit | Vulkan-driver and DXVK-version requirements still apply; proprietary NVIDIA and NVK are separate driver paths |
| AMD + Linux/Proton/DXVK | Same legacy chaining consideration | Same Wine-aware chaining; no AMD-specific wrapper API required | RADV/DXVK eligibility is distinct from an executed AMD result |
| Intel + Linux/Proton/DXVK | Same legacy chaining consideration | Same Wine-aware chaining; no Intel-specific wrapper API required | ANV/DXVK eligibility is distinct from an executed Intel result |
| NVIDIA 3D Vision / NVAPI stereo | Legacy stereo controls and stereo-parameter resources exist | Stereo control/resource path is stripped; this is not a replacement 3D Vision implementation | Legacy stereoscopy is different from rendering the game's ordinary 3D scene |
| Windows WARP | Software D3D backend can be used for a standalone probe if the DLL initializes successfully | Same category of standalone software-backend validation | A WARP result validates its software path, not physical NVIDIA/AMD/Intel behavior |
| Native Linux/macOS, native Vulkan, OpenGL or D3D12 wrapper | Not established by the compared D3D11 DLL | Not established by this Windows D3D11 product either | Proton/Wine translation does not turn the DLL into a native renderer for these APIs/platforms |

The NVIDIA-specific difference follows from the actual build graph and startup
code. Original 1.4.9 compiles `nvprofile.cpp`, links NVAPI, initializes NVAPI and
can configure stereo driver modes. Develop's project omits that compile/link
path, and its initialization proceeds without those calls.
[Original project](https://github.com/bo3b/3Dmigoto/blob/4ce5f2f72b2777223d2e809dcdafec514ac98295/DirectX11/DirectX11.vcxproj),
[original initialization](https://github.com/bo3b/3Dmigoto/blob/4ce5f2f72b2777223d2e809dcdafec514ac98295/DirectX11/D3D11Wrapper.cpp#L91),
[develop project](https://github.com/Fenris159/EDHM_2DMigoto/blob/5c74823ff0d21c662e2d659d04afc6624d245509/DirectX11/DirectX11.vcxproj),
[develop initialization](https://github.com/Fenris159/EDHM_2DMigoto/blob/5c74823ff0d21c662e2d659d04afc6624d245509/DirectX11/D3D11Wrapper.cpp#L99).

This does **not** prove that original shader replacement is NVIDIA-only:
the original caller does not use `InitializeDLL()`'s Boolean result to establish
such a rule. Non-NVIDIA success or failure must be observed rather than inferred
from an NVAPI error alone. NVIDIA itself defines NVAPI as access to its GPUs and
drivers on supported Windows systems.
[Original initialization caller](https://github.com/bo3b/3Dmigoto/blob/4ce5f2f72b2777223d2e809dcdafec514ac98295/DirectX11/D3D11Wrapper.cpp),
[NVIDIA NVAPI](https://developer.nvidia.com/nvapi/get-started).

Original's active context code also binds a stereo-parameter view and uses a
stereo handle. Develop's corresponding resource binding retains IniParams but
omits that stereo resource/control path. Removing this path does not imply that
every original scene texture was duplicated or that develop halves VRAM usage.
[Original resource binding](https://github.com/bo3b/3Dmigoto/blob/4ce5f2f72b2777223d2e809dcdafec514ac98295/DirectX11/HackerContext.cpp#L2577),
[develop resource binding](https://github.com/Fenris159/EDHM_2DMigoto/blob/5c74823ff0d21c662e2d659d04afc6624d245509/DirectX11/HackerContext.cpp#L3304).
NVIDIA ended Game Ready support for 3D Vision after its Release 418 generation;
ordinary D3D11 support and present-day stereo capability must not be conflated.
[NVIDIA support plan](https://nvidia.custhelp.com/app/answers/detail/a_id/4781).

## Renderer technologies are separate integrations

**None of the following SDK features is exercised or certified by this
comparison.** A successful D3D11 color/depth/compute probe does not establish an
upscaler, frame-generation or latency integration. The DLL does not add these
features merely by intercepting D3D11, and a GPU brand alone does not establish
their availability. The API descriptions below are scoped to the named SDK
generations, checked on the research date.

| Technology and reference version | Documented integration prerequisite | Honest conclusion for this comparison |
| --- | --- | --- |
| NVIDIA DLSS Super Resolution, Streamline **2.14.16** | D3D11, D3D12 or Vulkan integration, resource tagging and a successful adapter/driver feature query | A D3D11 integration is possible in the SDK; compatibility with either wrapper remains untested. [DLSS guide](https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.16/docs/ProgrammingGuideDLSS.md) |
| NVIDIA DLSS Frame Generation, Streamline **2.14.16** | D3D12 or Vulkan path, eligible adapter/driver and frame-generation input/presentation integration; this guide has no D3D11 FG path | This D3D11 wrapper does not supply that integration. The never-Present probe cannot validate frame generation. [FG guide](https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.16/docs/ProgrammingGuideDLSS_G.md#30-check-if-dlss-g-is-supported) |
| NVIDIA Reflex / NVAPI, Streamline **2.14.16** | Title-integrated latency markers and sleep calls, with runtime capability checks; its guide describes D3D11/D3D12/Vulkan device integration and conversion from native NVAPI calls | Removing the wrapper's legacy NVAPI stereo/profile path neither enables Reflex nor proves a game's own Reflex path broken or unaffected. [Reflex guide](https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.16/docs/ProgrammingGuideReflex.md) |
| AMD FSR upscaling: **FSR 1** versus FSR SDK **2.4.0** | FSR 1 documents D3D11/D3D12/Vulkan support. The newer SDK's upscalers, including 3.1.6 and 4.1.2, use its D3D12/Vulkan integration with effect-specific GPU requirements | FSR 1's D3D11 support is not evidence that every later FSR version is a D3D11 drop-in. Neither wrapper's FSR interoperability is tested here. [FSR 1](https://gpuopen.com/fidelityfx-superresolution/), [SDK 2.4.0](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/v2.4.0/docs/getting-started/index.md) |
| AMD FSR frame generation, FSR SDK **2.4.0** | D3D12/Vulkan integration of the relevant FG effect and frame-pacing/presentation path; 2.4.0 explicitly adds Vulkan parity for its FG effects | The SDK's Vulkan support is distinct from running an existing D3D11 title through DXVK. No FG or presentation interoperability is established. [Versioned changes](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/v2.4.0/Kits/FidelityFX/docs/whats-new/index.md) |
| Intel XeSS Super Resolution, XeSS **3.0.2** | Separate D3D11, D3D12 and Vulkan integrations. This SDK's D3D11 minimum specifies Intel Arc; the other APIs have their own hardware/extension requirements and cross-vendor paths | An Intel, AMD or NVIDIA name is insufficient to infer a supported API path, and the probe performs no XeSS-SR execution. [SR guide](https://github.com/intel/xess/blob/v3.0.2/doc/xess_sr_developer_guide_english.md#requirements) |
| Intel XeSS Frame Generation, XeSS **3.0.2** | D3D12, a working XeLL 1.3.0-or-newer integration and an FG proxy swapchain; the guide includes eligible Intel and non-Intel SM6.4 devices | This is not a D3D11 feature supplied by either wrapper. XeSS-FG, XeLL and their pacing behavior remain untested. [FG guide](https://github.com/intel/xess/blob/v3.0.2/doc/xess_fg_developer_guide_english.md#requirements) |
| NVIDIA 3D Vision | Legacy NVIDIA driver stereoscopy, with the support lifecycle and original/develop code distinction described above | Original's stereo controls and develop's removal are source findings; neither is a certification of ordinary game 3D, VR or modern FG. [NVIDIA support plan](https://nvidia.custhelp.com/app/answers/detail/a_id/4781) |

For Proton, a title's NVAPI/Reflex integration can also depend on DXVK-NVAPI or
the relevant Vulkan low-latency path. These are additional translation/integration
components, not capabilities implied by this DLL's lack of an NVAPI dependency.
[NVIDIA's Proton/Reflex guidance](https://docs.nvidia.com/datacenter/tesla/driver-installation-guide/latest/gaming.html#reflex).

## Proton and DXVK conditions

Develop detects Wine and applies a profile that disables library redirection
and foreground-window gating. With that profile active, it skips the legacy
`original_d3d11.dll` lookup and loads the prefix/system D3D11 implementation.
This is a concrete compatibility adaptation, not evidence of an arbitrary
performance percentage.
[Wine profile](https://github.com/Fenris159/EDHM_2DMigoto/blob/5c74823ff0d21c662e2d659d04afc6624d245509/DirectX11/WineCompat.cpp#L170),
[chaining](https://github.com/Fenris159/EDHM_2DMigoto/blob/5c74823ff0d21c662e2d659d04afc6624d245509/DirectX11/D3D11Wrapper.cpp#L318).

DXVK translates D3D11 to Vulkan and requires both its D3D11 and DXGI components.
The game-local EDHM wrapper and the prefix's translation DLL therefore occupy
different positions in the chain. Proton also offers `PROTON_USE_WINED3D` to
select its OpenGL-based alternative; a DXVK result does not validate that path.
[DXVK README](https://github.com/doitsujin/dxvk#how-to-use),
[Valve Proton options](https://github.com/ValveSoftware/Proton#runtime-config-options).

As checked on the research date, **DXVK 3.x** documents Vulkan 1.4-level
requirements plus specific required features and Wine 10.1 or newer. Its Linux
driver minimums are listed below. These are DXVK-version requirements, not new
requirements imposed by this wrapper, and must not be applied retroactively to
older Proton/DXVK releases.
[DXVK driver support](https://github.com/doitsujin/dxvk/wiki/Driver-support).

| DXVK 3.x Linux driver path | Documented minimum |
| --- | --- |
| AMD RADV | Mesa 25.0 |
| NVIDIA proprietary | 575.51.02 |
| NVIDIA NVK | Mesa 25.1 |
| Intel ANV | Mesa 25.1 |

Record the actual Proton/Wine, DXVK, Vulkan driver and device in each run's
manifest. The general matrix cannot certify all versions or GPU generations.

## Interpreting surfaces, memory and 3D interference

D3D resources contain geometry, textures or shader data. Views expose selected
interpretations/subresources; they should be counted separately from underlying
resources. `GetResource` returns the underlying resource with an additional COM
reference, so identity checks also need balanced ownership.
[Resource model](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-intro),
[subresources](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-subresources),
[view ownership](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11view-getresource).

| Observable focus | What it demonstrates | What it does not demonstrate |
| --- | --- | --- |
| Unique resource identities, with SRV/RTV/DSV/UAV counts separately | Which backing buffers/textures and view interfaces the fixture touches | Driver allocation size or actual VRAM residency |
| Create*View calls avoided with identical bound resource/descriptor | Less API object-creation work for that reference pattern | Automatically fewer texture copies, less VRAM, or better FPS |
| CopyResource / CopySubresourceRegion counts and requested bytes | Explicit data-movement requests in that fixture | Internal driver transfers, compression, tiling or memory bandwidth consumed |
| Staging allocations and readback Map calls | Explicit CPU-readback work added by that fixture | All synchronization or cache activity inside a driver |
| Identical draw arguments and expected color/depth/compute output | Preservation of the tested rendering result | Isolation from every game shader, resource or configuration |

Keep the probe's own output-verification copies and readbacks as explicit
controls. Attribute additional wrapper-generated operations separately; an
unchanged validation transfer is not wrapper overhead. Report process memory,
logical resource bytes and measured GPU residency as different quantities.

`CopyResource` requests a GPU copy of the whole resource. `Map` exposes CPU
access and its flags determine what happens when the GPU is still using the
resource. Consequently, avoiding view creation and avoiding GPU readback are
different benefits and require separate counters.
[CopyResource](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copyresource),
[Map](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map).

Develop forwards `DrawIndexed` between `BeforeDraw` and `AfterDraw`, with
configuration-controlled shader/resource work and skip handling. Its HUD focus
and removal of stereo support do not make it incapable of affecting the 3D
scene. A claim that it stays out of the way must name the tested configuration
and show that configuration's touched resources, output and additional work.
[Draw path](https://github.com/Fenris159/EDHM_2DMigoto/blob/5c74823ff0d21c662e2d659d04afc6624d245509/DirectX11/HackerContext.cpp#L936),
[DrawIndexed forwarding](https://github.com/Fenris159/EDHM_2DMigoto/blob/5c74823ff0d21c662e2d659d04afc6624d245509/DirectX11/HackerContext.cpp#L3600).

WARP is Microsoft's software rasterizer. Its timings characterize CPU/software
work and cannot establish physical-GPU performance or vendor-driver behavior.
[Microsoft WARP guide](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/directx-warp).

## Explicit limits

This appendix independently validates no physical NVIDIA, AMD or Intel Windows
or Linux driver combination. It also establishes no installed-game, VR,
stereoscopic, upscaler, ray-tracing, native macOS, wined3d or D3D12 compatibility
result. Use only executed cells from the main report for measured claims; all
other cells remain documented prerequisites or source-established behavior.
