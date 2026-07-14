## EDHM_2DMigoto 0.1.2

Patch release: live-play hardening for Elite Dangerous / EDHM inject DLL —
logging that can stay on without multi-GB spam, EDHM-matched theme poll
interval, safer crash handler, and quieter hot paths. InputLayout side-car fix
from 0.1.1 is retained.

### Added

- `[Logging] enabled=` opens `d3d11_log.txt` without requiring `calls=1`
- `[Logging] max_size_mb=` rotates oversized logs on open (default 32)
- Branch ahead/behind Shields badges (`main`/`develop`/`xxmi-base`)
- `upstream-xxmi` review-queue issue when the XXMI mirror advances

### Fixed

- `auto_refresh_file_to_monitor` polls every **2.0s** (EDHM shipped DLL parity;
  was 250ms)
- Crash handler ignores non-fatal **0x87A** (D3D debug layer) and similar codes
  (no SOS/dump storms with `crash=1` + Debug runtime)
- Safer unbuffered default when crash handling is enabled

### Changed

- Hot-path TextureOverride / Create*Shader logging is debug-only; Present log
  flush ~1 Hz unless `debug=1`
- Play-oriented `Dependencies/d3dx.ini` logging defaults
- Package / DLL file version **0.1.2**

### Notes

- Recommended play config: `enabled=1`, `calls=0`, `debug=0`, `unbuffered=0`,
  `crash=0`, `export_hlsl=0`, `dump_usage=0`
- Do not deploy a **Debug** build of this DLL for normal play
- Keep EDHM's own `d3dcompiler_47.dll` unless you intentionally replace it

Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
