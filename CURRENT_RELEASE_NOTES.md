## EDHM_2DMigoto 0.1.3-alpha.6

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.3-alpha.6] - 2026-08-12

### Added

- Ported XXMI 0.9.5–0.9.8 command-list and resource-pool updates: pool
  variables and spatial indexing, `CommandArgumentReader`, `store` rewrite,
  vertex-layout element format/offset commands, and expression additions
  (`frame_number`, `draw_number`, `dispatch_number`, binary literals, unary
  functions, static evaluation).
- Added `docs/ini-command-reference.md` covering the command-list language,
  including the newly ported XXMI commands.
- Added Qodo and SonarCloud pull-request check workflows for `main` and
  `xxmi-base`.

### Changed

- Restored the compiled DLL product name and copyright metadata to the
  upstream 3Dmigoto values.
- Clarified that EDHM_2DMigoto is the repository/build-package name while the
  runtime remains 3Dmigoto-derived software based on XXMI / 2Dmigoto.
- Reworded DLL diagnostics to identify the wrapper as 3Dmigoto and EDHM as its
  compatibility profile and configuration target.
- Addressed first-party SonarCloud blockers and high-risk smells (`nullptr`,
  safer string helpers, compiled shader blobs excluded from PHP scans).

### Fixed

- Applied input-layout overrides on every draw with correct AddRef/Release
  ownership, and restored deferred overrides even when the override already
  matched the current layout.
- Kept `persistent_variables` in sync when pool metadata copies a `PERSIST`
  flag, and stopped treating `wcstof` underflow (for example `1e-50`) as
  infinity.
- Included all four spatial-hash inputs in the L2 cache key, rejected
  overflowing region-size math, and freed the temporary readback scratch
  buffer after the CPU snapshot is copied.
- Made `random()`’s shared call counter atomic for deferred-context use.

### Notes

- Package pre-release version is **0.1.3-alpha.6**; DLL file/product version
  remains **0.1.3**.
- Elite still receives real `ID3D11InputLayout*` pointers. Layout override
  commands bind `GetOrigInputLayout()` from the side-car cache; XXMI’s COM
  wrapper return path is not used.
- Present timing remains `GetTickCount64()`.
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
