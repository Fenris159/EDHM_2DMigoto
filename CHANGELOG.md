# Changelog

All notable changes to **EDHM_2DMigoto** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Package / release tags use this file and the root `VERSION` file
(`vMAJOR.MINOR.PATCH` for releases, `vMAJOR.MINOR.PATCH-alpha.N` /
`-beta.N` / `-rc.N` for pre-releases).

DLL file properties (`version.h`) match package SemVer (same MAJOR.MINOR.PATCH as
`VERSION`). Behaviour still targets classic 3Dmigoto / EDHM compatibility.

## [Unreleased]

### Changed

- Restored the compiled DLL product name and copyright metadata to the
  upstream 3Dmigoto values.
- Clarified that EDHM_2DMigoto is the repository/build-package name while the
  runtime remains 3Dmigoto-derived software based on XXMI / 2Dmigoto.

### Planned

- Further EDHM smoke validation against stock 3Dmigoto / EDHM packages
- Optional: gate remaining chatty LogInfo behind debug / a dedicated log level

## [0.1.3-alpha.3] - 2026-07-15

### Changed

- Streamlined `Dependencies/d3dx.ini` around EDHM and Elite Dangerous runtime
  use, replacing stale XXMI-loader and non-EDHM game comments with EDHM-focused
  guidance.
- Aligned stable sample config defaults with the installed Odyssey layout,
  including the standard include tree, active theme auto-refresh example,
  logging defaults, cursor handling, profile defaults, and shader override
  examples.
- Added `stereo_params` parsing support for decompiled shader declarations and
  hardened `auto_refresh_file_to_monitor` path handling for EDHM theme reloads.

### Fixed

- Corrected non-fatal exception filtering so continuable diagnostic exceptions
  are ignored consistently.
- Suppressed inherited DirectXTK C5045 warning noise in the bundled DirectXTK
  project file.

### Notes

- Includes all **0.1.3-alpha.2** DX11 hardening. The bundled vendor cache
  remains pinned to the last published release DLL until the release workflow
  publishes `v0.1.3-alpha.3` and refreshes `vendor/edhm-runtime/`.

## [0.1.3-alpha.2] - 2026-07-14

### Changed

- DX11 type-safety / resource-management hardening (context, device, resource
  hash, ini paths) on top of the alpha.1 audit fixes.
- Package pre-release version is **0.1.3-alpha.2**; DLL file/product version
  remains **0.1.3**.

### Notes

- Includes all **0.1.3-alpha.1** fixes (Present1 logging, hunting empty-set
  guards, crash-handler resume, InputLayout `SetPrivateDataInterface` ownership,
  `skip_early_includes_load=0` sample, warning cleanup).
- Alpha train for live Elite smoke testing. Prefer Release DLL; keep
  `export_hlsl=0` and `dump_usage=0` for normal play.

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

## [0.1.2] - 2026-07-13

### Added

- `[Logging] enabled=` opens `d3d11_log.txt` for startup / errors / reloads without
  requiring `calls=1` (which historically was the only way to open the log file)
- `[Logging] max_size_mb=` rotates oversized logs to `d3d11_log.txt.prev` on open
  (default 32)
- Shields.io endpoint badges for `main` vs `develop` and `develop` vs `xxmi-base`,
  refreshed by the Branch status badges workflow
- Update xxmi-base Action upserts an evergreen GitHub issue labeled `upstream-xxmi`
  when the XXMI mirror advances

### Fixed

- **`auto_refresh_file_to_monitor` poll interval**: use **2.0s** to match EDHM’s
  shipped `d3d11.dll` (was 250ms / ~4 Hz — too aggressive for live Present)
- Crash handler no longer treats D3D debug-layer break **`0x87A`** (and similar
  non-fatal codes such as SetThreadName / OutputDebugString) as process crashes;
  avoids SOS-beep storms and dump spam when `crash=1` meets a Debug build or
  debug runtime
- Unbuffered logging defaults on when `crash=1` so last log lines survive hard faults

### Changed

- Hot-path logging quieted for live Elite play: TextureOverride match spam and
  Create*Shader chatter are `LogDebug` only (`debug=1`); Present log flush is
  ~1 Hz (or every frame when `debug=1`) instead of every Present
- Default template `Dependencies/d3dx.ini` play profile: `enabled=1`, `calls=0`,
  `debug=0`, `export_hlsl` / hunting notes, `crash=0` for normal sessions
- Docs: `docs/upstream-sync.md` review-queue issue + badge semantics; patch notes
  for auto_refresh interval
- Package / DLL file version **0.1.2**

### Notes

- Recommended **play** logging: `enabled=1`, `calls=0`, `debug=0`, `unbuffered=0`,
  `crash=0`, `export_hlsl=0`, `dump_usage=0`. Use `crash=1` only for short
  diagnostic sessions (with Release DLL).
- Do not deploy a **Debug** build of this DLL to the live game folder: Debug
  enables the D3D debug layer and threading opts that tank performance.
- InputLayout side-car fix from **0.1.1** is unchanged; this release focuses on
  live-play logging, EDHM theme poll parity, and crash-handler safety.

## [0.1.1] - 2026-07-13

### Fixed

- **Crash on location load** (ACCESS_VIOLATION in `System32\d3d11.dll`): stop
  returning XXMI’s `HackerInputLayout` COM wrapper to the game. Layouts are
  cached as a side-car on the real D3D object (private data); the game always
  receives and binds genuine `ID3D11InputLayout` pointers. Side-car no longer
  `Release()`s the game’s Create ref (double-free risk).
- `CreateRasterizerState1` now honours `rasterizer_disable_scissor` the same way
  as `CreateRasterizerState` (Elite may use the DESC1 path).
- Release workflow: detect existing git tags via exit code / `ls-remote` instead
  of broken PowerShell `if (git rev-parse …)` (false “tag already exists”).

### Changed

- Frame analysis resolves layout cache via `FromLayout` after `IAGetInputLayout`
  (compatible with real layout pointers).
- **`version.h` file/product version aligned with package SemVer** (`0.1.1`)
  instead of classic 3Dmigoto `1.4.5` labeling.

### Notes

- EDHM with large IniParams sets (300+ float4 slots) still works; the optional
  on-screen “exceeds recommended 256” notice is a soft warning and is suppressed
  when `show_warnings=0`.
- Recommended play config with software/EDHM cursor paths: `hide_cursor=1`.
- Carrier management letterboxing/border of 3D world is present with stock
  3Dmigoto as well (not treated as a regression).

## [0.1.0] - 2026-07-13

### Added

- Independent repository based on SpectrumQT XXMI-Libs-Package (2Dmigoto), not a GitHub fork
- Tracking remotes for `xxmi` and `3dmigoto`; `xxmi-base` mirror branch + update Action
- Slim `develop` tree focused on building `d3d11.dll` for EDHM
- EDHM compatibility bridge (classic include load, hash TextureOverride precedence,
  `auto_refresh_file_to_monitor`, safer InputLayout unwrap)
- Scaffold `main` for release merges; docs for build, layout, upstream sync
- Release / pre-release GitHub Actions, CI build, changelog, and release notes pipeline
- `vendor/edhm-runtime/` cache updated automatically by the Release workflow (Git LFS for `d3d11.dll`)

### Changed

- Product name in `version.h`: **EDHM_2DMigoto** (file version was still labeled
  1.4.5 for tooling familiarity; package SemVer is `VERSION`)

### Notes

- `d3dcompiler_47.dll` remains the Microsoft redistributable (SDK / EDHM package)
- Stereo features remain stripped (2Dmigoto); XXMI performance-oriented code paths retained

[Unreleased]: https://github.com/Fenris159/EDHM_2DMigoto/compare/v0.1.3-alpha.3...HEAD
[0.1.3-alpha.3]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.3-alpha.3
[0.1.3-alpha.2]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.3-alpha.2
[0.1.3-alpha.1]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.3-alpha.1
[0.1.2]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.2
[0.1.1]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.1
[0.1.0]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.0
