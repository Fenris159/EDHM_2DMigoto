## EDHM_2DMigoto 0.1.3-alpha.2

Alpha.2 pre-release on the 0.1.3 train: carries alpha.1 runtime-safety audit
fixes plus a follow-up DX11 type-safety / resource-management refactor.

### Changed (since alpha.1)

- DX11 type-safety and resource-management hardening (context, device, resource
  hash, ini paths)
- Package pre-release version **0.1.3-alpha.2** (DLL file version remains **0.1.3**)

### Included from alpha.1

- Present1 no longer force-enables global debug logging every frame
- Hunting previous-buffer selection guards empty visited sets
- `crash=1` executes the exception handler after dumps (does not resume ordinary faults)
- InputLayout side-cars owned via `SetPrivateDataInterface` (game still gets real layouts)
- Context layout cache released on ClearState / teardown paths
- Sample `skip_early_includes_load = 0` for classic EDHM include load

### Notes

- For live Elite smoke testing with a **Release** build
- Play config: `export_hlsl=0`, `dump_usage=0`, `debug=0`, `crash=0` for normal sessions
- Do not deploy a Debug DLL for daily play

Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
