## EDHM_2DMigoto 0.1.7

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.7] - 2026-11-15

### Fixed

- Pass D3D12 command-queue swap chains through all four DXGI creation APIs
  without D3D11 overrides or wrappers. This preserves `IDXGISwapChain4` for
  EDUP's XeSS Frame Generation helper and avoids the false DirectX 12 warning
  and extra D3D11On12 device. D3D11 game swap chains retain EDHM behavior.
  Thanks to [Duceowen for the report and patch](https://github.com/Fenris159/EDHM_2DMigoto/issues/52).

### Added

- Off-game D3D12 WARP checks for native swap-chain interfaces and dimensions,
  unchanged creation failures, warning-free helper initialization, and D3D11
  resource tracking after helper calls. The 0.1.6 code reproduces the reported
  `0x80004002` interface failure on x64 and Win32.
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
