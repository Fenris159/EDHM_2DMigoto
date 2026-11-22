## EDHM_2DMigoto 0.1.9

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.9] - 2026-11-22

### Fixed

- Preserve requested D3D11.1+ and D3D12.x feature levels during
  `D3D11CreateDevice` and `D3D11CreateDeviceAndSwapChain` instead of
  downgrading them to `11.0`, while filtering down-level (`< 11.0`) fallback
  entries in modes `0` and `2` and stepping down unsupported newer enums on
  `E_INVALIDARG` in mode `2` without skipping supported intermediate levels.
- Default `allow_platform_update` to enabled so `ID3D11Device1`,
  `ID3D11DeviceContext1`, and `IDXGIFactory2` remain available out of the box;
  preserve modern `IDXGIFactory4+` interfaces in `Hooked_CreateDXGIFactory2`
  and pass through `IDXGISwapChain4` for HDR10 metadata and upscaler/frame-gen
  consumers.
- Complete `ID3D11Device1` and `ID3D11DeviceContext1` hook/trampoline coverage,
  synchronize hooked wrapper map removal on release, complete lazy
  `GetImmediateContext1` resource binding and `[Constants]` initialization,
  and preserve up to 64 UAV slots on FL 11.1+ devices in OM state save/restore
  and frame-analysis UAV dumps.
- Disable `LoadLibraryExW` redirection from `DllMain` onward when loaded under
  a chained module name (such as `d3d11_edhm.dll`) so system/SDK probes for
  `System32\d3d11.dll` are never redirected back to an outer proxy.
- Scope forwarded-Present suppression to its swap-chain wrapper, allowing an
  independent primary frame to progress while another device has an in-flight
  secondary proxy call.

### Added

- Windows WARP regression coverage for D3D11.1+/12.0 feature-level
  preservation, mixed/down-level fallback filtering, mode `2` `E_INVALIDARG`
  step-down, `ID3D11Device1`/`ID3D11DeviceContext1` hooked and lazy-first
  `[Constants]` initialization, renamed-DLL `LoadLibraryExW` pass-through, and
  64-slot FL 11.1 OM UAV state save/restore.
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
