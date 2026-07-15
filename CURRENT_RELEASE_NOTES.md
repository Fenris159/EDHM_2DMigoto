## EDHM_2DMigoto 0.1.3-alpha.3

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.

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
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
