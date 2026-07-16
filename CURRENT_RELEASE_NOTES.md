## EDHM_2DMigoto 0.1.3-alpha.4

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.3-alpha.4] - 2026-07-16

### Added

- Added Wine/Proton host detection and a startup compatibility report containing
  effective System settings, DLL paths, DLL overrides, conflicting Proton
  renderer options, and Flatpak environment detection.
- Added standard Flatpak Steam setup instructions for both physical EDHM
  directories and narrowly permitted EDHM-UI symlink targets.

### Changed

- Wine compatibility now uses the standard Proton/Wine-prefix D3D11 chain with
  no renamed local DXVK DLLs or `proxy_d3d11` configuration.
- The automatic Wine profile is limited to `load_library_redirect=0` and
  `check_foreground_window=0`; initialization delays remain explicit opt-in
  troubleshooting settings.
- Moved remaining successful shader and Texture3D creation messages behind
  `debug=1` and made recursive include patterns and ini values UTF-8 safe.
- Re-aligned `Dependencies/d3dx.ini` with the `xxmi-base` section order and
  examples, restored standard recursive exclusions, and added concise guidance
  for choosing EDHM, Windows, Wine/Proton, proxy, hashing, and logging settings.
- Added `linux-compatibility` as an explicit pre-release source in the manual
  GitHub Actions release workflow.
- Enabled compiler stack-cookie protection for all Release and Zip Release
  runtime, decompiler, and bundled DirectXTK configurations.
- Added the Linux compatibility guide to repository navigation and release ZIPs.

### Fixed

- Reject invalid chained D3D11 modules before their missing exports can be
  called, guard optional D3D11 exports, and serialize one-time initialization.
- Made one-call D3D11 redirect suppression thread-local and initialized its TLS
  storage before installing hooks.
- Prevented recursive include scanning from reading before short filenames and
  added explicit diagnostics for inaccessible include and ShaderFixes paths.
- Stopped scissor overrides from modifying caller-owned `const` rasterizer
  descriptors in either D3D11 rasterizer-state creation path.
- Prevented a full one-slot region hash table from looping forever on a missing
  key, bounded probing, and removed end-offset truncation during invalidation.
- Fixed a debug-layer null dereference and released debug interfaces after use.
- Stopped device creation from modifying the game's feature-level array and
  initialized optional device and swap-chain outputs before forwarding calls.
- Hardened frame analysis, shader cache and live reload paths against null
  metadata, malformed files, unbounded first-line scans, and invalid timestamps.
- Fixed optional shader decompiler error output using a null file and treating a
  C runtime `FILE*` as a Win32 handle.
- Avoided a zero-count UAV state query in the overlay path and validated
  embedded font resources without leaking device/context references on failure.
- Hardened DLL-relative path construction, blank `user_config` handling,
  profile-helper paths, concurrent crash-exception counting, COM output handling,
  and warning-prone numeric conversions.

### Notes

- Package pre-release version is **0.1.3-alpha.4**; DLL file/product version
  remains **0.1.3**.
- The bundled vendor cache remains pinned to the last published release DLL
  until the release workflow publishes `v0.1.3-alpha.4` and refreshes it.
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
