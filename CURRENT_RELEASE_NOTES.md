## EDHM_2DMigoto 0.1.3-alpha.2

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
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
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
