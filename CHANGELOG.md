# Changelog

All notable changes to **EDHM_2DMigoto** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Package / release tags use this file and the root `VERSION` file
(`vMAJOR.MINOR.PATCH` for releases, `vMAJOR.MINOR.PATCH-beta.N` / `-rc.N` for pre-releases).

DLL file properties (`version.h`) may still report a classic 3Dmigoto-compatible
file version (e.g. 1.4.5) for identity with EDHM tooling; that is separate from
the package version below.

## [Unreleased]

### Added

- `vendor/edhm-runtime/` cache updated automatically by the Release workflow (Git LFS for `d3d11.dll`)

### Planned

- First full merge of `develop` into `main` for a stable release cut
- In-game EDHM smoke validation against stock 1.4.5 DLL

## [0.1.0] - 2026-07-13

### Added

- Independent repository based on SpectrumQT XXMI-Libs-Package (2Dmigoto), not a GitHub fork
- Tracking remotes for `xxmi` and `3dmigoto`; `xxmi-base` mirror branch + update Action
- Slim `develop` tree focused on building `d3d11.dll` for EDHM
- EDHM compatibility bridge (classic include load, hash TextureOverride precedence,
  `auto_refresh_file_to_monitor`, safer InputLayout unwrap)
- Scaffold `main` for release merges; docs for build, layout, upstream sync
- Release / pre-release GitHub Actions, CI build, changelog, and release notes pipeline

### Changed

- Product identity in `version.h`: EDHM_2DMigoto 1.4.5 (file version; package version is `VERSION`)

### Notes

- `d3dcompiler_47.dll` remains the Microsoft redistributable (SDK / EDHM package)
- Stereo features remain stripped (2Dmigoto); XXMI performance-oriented code paths retained

[Unreleased]: https://github.com/Fenris159/EDHM_2DMigoto/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.0
