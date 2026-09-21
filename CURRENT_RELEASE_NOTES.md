## EDHM_2DMigoto 0.1.3

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.3] - 2026-09-21

### Added

- Optional two-level shader context hunting: keep a selected pixel or vertex
  shader and cycle the distinct draw contexts that use it, then mark a
  conservative `TextureOverride` candidate plus its parent `ShaderOverride`.
  Disabled by default (`advanced_hunting=0`); see
  `docs/advanced-shader-hunting.md`.

### Changed

- Promoted the 0.1.3-alpha train to a stable package tag. DLL file/product
  version remains **0.1.3**.
- Hardened hunting resource-state updates so setter-triggered refreshes run
  only while a context-hunting session is active; the initial snapshot still
  runs before the session starts.
- Continued native quality modernization and SonarCloud history
  reconciliation on the maintained DX11/EDHM paths.

### Fixed

- Context hunting reports now switch back to decimal after writing the hex
  resource hash, so `match_first_*` values stay numeric when `index_count` is 0.
- Release `sync-notes` no longer fails when `CHANGELOG.md` contains blank
  lines (PowerShell 7 rejected empty strings on a mandatory `[string[]]`).

### Notes

- Latest previous package tag was **0.1.3-alpha.6**. This release is the
  stable 0.1.3 cut of that train plus context hunting.
- Hunting Level 2 is for shader authors. Normal play configs can leave
  `advanced_hunting=0`.
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
