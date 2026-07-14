## EDHM_2DMigoto 0.1.3-alpha.1

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.3-alpha.1] - 2026-07-14

### Fixed

- `IDXGISwapChain1::Present1` no longer force-enables global debug logging
  every frame; `Present1` now follows the normal `debug=1` path.
- Shader hunting no longer dereferences the previous visited index/vertex
  buffer when the visited-buffer set is empty.
- Crash handler level `crash=1` now executes the exception handler after
  writing diagnostics instead of resuming the same faulting instruction.
- `HackerInputLayout` side-cars are now owned through D3D private
  `IUnknown` data (`SetPrivateDataInterface`) instead of a raw pointer blob.
  The game still receives the real `ID3D11InputLayout*`.
- Context-side cached input-layout metadata is released on `ClearState`,
  command-list state clear paths, and context teardown.

### Changed

- Develop template `Dependencies/d3dx.ini` keeps
  `skip_early_includes_load = 0` so EDHM includes are available during DLL
  initialization like classic 3Dmigoto.
- Cleaned audit-targeted build warnings around hunting buffer selection,
  resource-hash logging format strings, and the C++17-only `if constexpr`
  helper.
- Package pre-release version is **0.1.3-alpha.1**; DLL file/product version
  is **0.1.3**.

### Notes

- Alpha release candidate for live Elite smoke testing. Validate cold start,
  station approach/dock, F11/theme reload, and quiet `debug=0` logging before
  promoting this train.
- Keep normal play configs at `export_hlsl=0` and `dump_usage=0`.
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
