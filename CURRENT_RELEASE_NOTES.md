## EDHM_2DMigoto 0.1.1

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
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
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
