## EDHM_2DMigoto 0.1.0

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
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
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
