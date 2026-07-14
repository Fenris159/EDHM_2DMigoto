## EDHM_2DMigoto 0.1.2

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.2] - 2026-07-13

### Added

- `[Logging] enabled=` opens `d3d11_log.txt` for startup / errors / reloads without
  requiring `calls=1` (which historically was the only way to open the log file)
- `[Logging] max_size_mb=` rotates oversized logs to `d3d11_log.txt.prev` on open
  (default 32)
- Shields.io endpoint badges for `main` vs `develop` and `develop` vs `xxmi-base`,
  refreshed by the Branch status badges workflow
- Update xxmi-base Action upserts an evergreen GitHub issue labeled `upstream-xxmi`
  when the XXMI mirror advances

### Fixed

- **`auto_refresh_file_to_monitor` poll interval**: use **2.0s** to match EDHM’s
  shipped `d3d11.dll` (was 250ms / ~4 Hz — too aggressive for live Present)
- Crash handler no longer treats D3D debug-layer break **`0x87A`** (and similar
  non-fatal codes such as SetThreadName / OutputDebugString) as process crashes;
  avoids SOS-beep storms and dump spam when `crash=1` meets a Debug build or
  debug runtime
- Unbuffered logging defaults on when `crash=1` so last log lines survive hard faults

### Changed

- Hot-path logging quieted for live Elite play: TextureOverride match spam and
  Create*Shader chatter are `LogDebug` only (`debug=1`); Present log flush is
  ~1 Hz (or every frame when `debug=1`) instead of every Present
- Default template `Dependencies/d3dx.ini` play profile: `enabled=1`, `calls=0`,
  `debug=0`, `export_hlsl` / hunting notes, `crash=0` for normal sessions
- Docs: `docs/upstream-sync.md` review-queue issue + badge semantics; patch notes
  for auto_refresh interval
- Package / DLL file version **0.1.2**

### Notes

- Recommended **play** logging: `enabled=1`, `calls=0`, `debug=0`, `unbuffered=0`,
  `crash=0`, `export_hlsl=0`, `dump_usage=0`. Use `crash=1` only for short
  diagnostic sessions (with Release DLL).
- Do not deploy a **Debug** build of this DLL to the live game folder: Debug
  enables the D3D debug layer and threading opts that tank performance.
- InputLayout side-car fix from **0.1.1** is unchanged; this release focuses on
  live-play logging, EDHM theme poll parity, and crash-handler safety.
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
