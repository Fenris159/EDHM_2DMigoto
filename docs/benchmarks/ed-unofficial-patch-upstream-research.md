# ED Unofficial Patch: upstream compatibility and performance evidence

The [combined source assessment](../ed-unofficial-patch-compatibility.md)
compares these findings with EDHM_2DMigoto's actual loading and wrapping paths.

**Checked 2026-11-13.** This note records upstream documentation, source and
issue evidence. It does not claim a combined game run or benchmark of
EDHM_2DMigoto. Our comparison target is develop
`4c953f29127b1d13301c733f157f0850cc2eb309`; its runtime is unchanged from the
previously measured `5c74823f`. ED-UP was inspected at
[`aa765cd9b90d9b3027c6505157c607d151eeaef5`](https://github.com/characterecho-sean/ed-unofficial-patch/commit/aa765cd9b90d9b3027c6505157c607d151eeaef5).
All field observations below belong to upstream or its contributors, rather
than to this investigation.

## Release versus development source

The newest published non-prerelease is
[v0.22.0](https://github.com/characterecho-sean/ed-unofficial-patch/releases/tag/v0.22.0),
published **2026-11-10 01:22:28 UTC**, made for Odyssey build **334925**. Its
annotated tag `c6b90948333c899968289870127878988eae539e` resolves to
`ba02b6be7003d3c92aa4ee7f1bfc134aeeab084d`. The inspected main revision is
[18 commits ahead](https://github.com/characterecho-sean/ed-unofficial-patch/compare/ba02b6be7003d3c92aa4ee7f1bfc134aeeab084d...aa765cd9b90d9b3027c6505157c607d151eeaef5).

The substantive post-release AA repair lets flat deferred lighting choose the
correct camera when a held tool and the world contribute distinct source
records at render size equal to or greater than output. Other changes concern
compressed diagnostics, rotating logs, packaging, build validation and a shared
schema-2 installer/launcher engine. These are not evidence that v0.22.0 lacks a
mandatory new EDHM coexistence patch.
[Held-tool repair](https://github.com/characterecho-sean/ed-unofficial-patch/commit/1b924faba0ec328582d1d058055422134454df1d),
[comparison](https://github.com/characterecho-sean/ed-unofficial-patch/compare/ba02b6be7003d3c92aa4ee7f1bfc134aeeab084d...aa765cd9b90d9b3027c6505157c607d151eeaef5).

Native on-foot VR and the Oculus-route redirection retain exact-game-build
requirements; other hooks can stand down individually after a game update.
An AA refusal after an update therefore need not implicate an EDHM DLL.
[Release notes](https://github.com/characterecho-sean/ed-unofficial-patch/releases/tag/v0.22.0),
[AA issue #128](https://github.com/characterecho-sean/ed-unofficial-patch/issues/128).

## What the AA engines actually implement

| Choice | Implementation and important boundary |
| --- | --- |
| `on` | ED-UP's own temporal AA; temporal depth/motion/history processing rather than a free replacement for no AA. |
| `dlss` | NVIDIA NGX/DLSS with `nvngx_dlss.dll` and an eligible RTX device. Without the required runtime/device, the documented fallback is ED-UP TAA. Full render size is the DLAA quality case. |
| `fsr` | FSR **3.1 upscaling**, integrated through the community FidelityFX-SDK-DX11 port and compiled into the patch. It is not the official newer FSR SDK's native D3D11 backend. |
| `xess` | XeSS-SR, implemented here with Intel's **D3D12 SDK over shared D3D11/D3D12 devices**. This is not the narrower hardware eligibility of Intel's native D3D11 SDK interface. |

The live mode parser enables the same temporal producers in flat and VR, with
separate feature slots. Source documents shared textures, bridge copies,
synchronization and separate bridge timing for XeSS. That creates additional
device-interface, driver and translation requirements beyond loading a D3D11
proxy. Successful DLSS/FSR execution does not establish XeSS execution.
[Mode parser](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/src/common/temporal_mode.h),
[XeSS interface](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/src/d3d11/xess_engine.h),
[XeSS implementation](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/src/d3d11/xess_engine.cpp),
[FSR implementation](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/src/d3d11/fsr3_engine.h).

**Upscaling is not frame generation.** The XeSS interface explicitly excludes
frame generation. Release references to OFXR-Bridge/VectorXR describe external
OpenXR API layers and a reference-space stall repair, not ED-UP supplying
DLSS-G, FSR frame generation or XeSS-FG.
[XeSS interface](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/src/d3d11/xess_engine.h),
[release performance section](https://github.com/characterecho-sean/ed-unofficial-patch/releases/tag/v0.22.0).

## EDHM composition is intentional, with version-sensitive boundaries

The current manual instructs ED-UP to own `d3d11.dll` and load the existing
EDHM DLL under a renamed filename through `[advanced] real_dll`. The settings
decision is shared across profiles: `edvr.ini` supplies the chain to VR and
flat; a value in `edvr-flat.ini` can override it, including `none` to disable
the chain. This makes an old flat override a plausible cause of differing
behavior between modes.
[Manual installation](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/docs/manual-install.md#running-alongside-edhm-by-hand),
[profile selection](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/src/common/real_dll_choice.h).

The troubleshooting page still contains an incompatible-looking `[chain]`
example. Use the current manual and implemented `advanced.real_dll` decision
instead of copying that older snippet. The main documentation also notes that
EDHM's uninstall can delete the ED-UP file occupying the shared filename, and
that installer repair restores the detected chain.
[Troubleshooting](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/docs/troubleshooting.md),
[manual](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/docs/manual-install.md#running-alongside-edhm-by-hand).

EDHM is not an unknown mod to this code. Its flat projection recipes include
audited EDHM pixel-shader variants that read `t120`, and its UI-layer guard
recognizes a measured EDHM tonemap variant plus physically checked alternatives.
This supports intended composition, without certifying every shader pack,
settings tier or future replacement shader.
[Projection recipes](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/src/d3d11/flat_projection_recipes.h#L342),
[flat UI guard](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/src/d3d11/flat_ui_layer_math.h#L110).

## Concrete historical conflicts and present uncertainty

- **HUD swim/ghosting:** issue #90 isolated missing UI-depth coverage with
  EDHM on v0.19.1. The maintainer marked it fixed in **0.20.0 RC2**. Current
  source includes EDHM variants, so this is a historical failure mechanism and
  a remaining compatibility boundary, not proof that current EDHM always fails.
  [Issue #90](https://github.com/characterecho-sean/ed-unofficial-patch/issues/90).
- **EDHM + ReShade losing theme/Present processing:** merged PR #111 gates an
  optional `IDXGISwapChain3` probe so it cannot discard a third-party swapchain
  wrapper. It merged **2026-10-19** and predates v0.22.0. The contributor reports
  successful flat, Quest 3/VDXR and SteamVR combined sessions; those observations
  do not name this fork's current binary.
  [PR #111](https://github.com/characterecho-sean/ed-unofficial-patch/pull/111),
  [current guard](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/src/d3d11/swapchain_probe.h).
- **ReShade loader guard:** upstream documents a separate dynamic-load failure
  leaving EDHM's IniParams table empty. Preloading system DLLs and IAT hooks
  on the chained module/executable are present in current source. This is
  independent of the swapchain repair above.
  [Investigation](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/docs/reshade-loadlibrary-deadlock-2026-10-15.md),
  [implementation](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/src/d3d11/d3d11_proxy.cpp#L158).
- **Unresolved reports:** #118 remains open with black rectangles/theme reports.
  One reporter said an EDHM-UI update plus reinstall resolved their case;
  the maintainer requested logs. This is a user report, not a proven defect
  in EDHM_2DMigoto or a universal current-release incompatibility.
  [Issue #118](https://github.com/characterecho-sean/ed-unofficial-patch/issues/118).

## Platform and performance interpretation

The project distributes a Proton-oriented Linux installer and documents
standalone flat operation. VR uses its own OpenXR runtime reached through
Elite's OpenVR entry point; native Oculus/LibOVR is not a supported fallback.
The author names field checks on Pimax/PiOpenXR and Quest/VDXR/Air Link, not
every headset, runtime or Linux VR stack.
[Pinned README](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/README.md#headsets-and-vr-runtimes).

Potential upscaling savings come from rendering fewer scene pixels. In VR,
use HMD Image Quality below 1.0 while leaving Supersampling at 1.0; flat uses
Supersampling for scene size. At full render size DLSS is DLAA: quality can
improve while temporal processing adds work. UI quality independently enlarges
the sharp UI layer, and optional HUD AA adds GPU work and approximately
10 logical bytes per UI pixel per eye. Neither is a free performance setting.
[Current feature/settings guide](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/docs/fixes.md#upscaling-dlss-fsr-and-xess).

The v0.22.0 author reports **1.4–1.75 ms less ED-UP CPU work per VR frame**
relative to the previous release. That is an upstream improvement claim,
not this DLL's contribution or a combined-build benchmark. Older performance
notes mix different stock EDHM builds, settings, GPUs and render paths and
must not be imported as measurements of our develop build.
[Release notes](https://github.com/characterecho-sean/ed-unofficial-patch/releases/tag/v0.22.0),
[explicit comparison limits in the older review](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/docs/dlss-performance-review-2026-09-28.md).

Proton also has a concrete historical cost: mapped constant-buffer reads were
very slow under DXVK on a contributor's AMD system. Issue #65 was closed after
the contributor verified the adaptive cache in **0.18.1**. The cache remains
in current source with automatic selection; historical forcing keys/workarounds
are not current setup instructions. This establishes a backend-sensitive risk
and mitigation, not a required penalty introduced by our wrapper.
[Issue #65](https://github.com/characterecho-sean/ed-unofficial-patch/issues/65#issuecomment-6011339375),
[adaptive mapping implementation](https://github.com/characterecho-sean/ed-unofficial-patch/blob/aa765cd9b90d9b3027c6505157c607d151eeaef5/src/d3d11/flat_map_bounce.h).

The supported conclusion is intentional coexistence with known compatibility
work, plausible GPU-side savings when scene resolution is reduced, and added
CPU/GPU/memory work for reconstruction and UI handling. No source or issue here
quantifies the net result of ED-UP combined with this fork's exact develop DLL.
