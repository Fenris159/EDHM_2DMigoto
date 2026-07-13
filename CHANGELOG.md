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

[Unreleased]: https://github.com/Fenris159/EDHM_2DMigoto/compare/v0.1.1...HEAD
[0.1.1]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.1
[0.1.0]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.0
