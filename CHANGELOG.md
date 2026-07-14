# Changelog

All notable changes to **EDHM_2DMigoto** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Package / release tags use this file and the root `VERSION` file
(`vMAJOR.MINOR.PATCH` for releases, `vMAJOR.MINOR.PATCH-beta.N` / `-rc.N` for pre-releases).

DLL file properties (`version.h`) match package SemVer (same MAJOR.MINOR.PATCH as
`VERSION`). Behaviour still targets classic 3Dmigoto / EDHM compatibility.

## [Unreleased]

### Planned

- Further EDHM smoke validation against stock 3Dmigoto / EDHM packages
- Optional: gate remaining chatty LogInfo behind debug / a dedicated log level

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

[Unreleased]: https://github.com/Fenris159/EDHM_2DMigoto/compare/v0.1.2...HEAD
[0.1.2]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.2
[0.1.1]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.1
[0.1.0]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.0
