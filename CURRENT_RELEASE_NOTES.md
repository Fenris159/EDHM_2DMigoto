## EDHM_2DMigoto 0.1.1

Patch release: fix Elite Dangerous crash on location load caused by XXMI-style
InputLayout COM wrapping. EDHM HUD shaders and classic TextureOverride behaviour
from 0.1.0 are retained.

### Fixed

- Crash during open-world / location load (`ACCESS_VIOLATION` in system `d3d11.dll`):
  game now always receives real `ID3D11InputLayout` objects; layout metadata is a
  private-data side-car only
- `CreateRasterizerState1` respects `rasterizer_disable_scissor` (parity with the
  legacy CreateRasterizerState path)
- Release workflow false positive “Tag already exists” on Windows runners

### Notes

- DLL file version remains **1.4.5** (`version.h`) for EDHM tooling familiarity;
  package tag is **v0.1.1**
- For in-game cursor only (no OS pointer): set `hide_cursor=1` under `[Device]`
- Keep EDHM’s own `d3dcompiler_47.dll` unless you intentionally replace it

Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
