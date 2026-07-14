## EDHM_2DMigoto 0.1.3-alpha.1

Alpha release candidate for validating the audit hardening pass in live Elite
Dangerous / EDHM sessions.

### Fixed

- `Present1` no longer force-enables global debug logging every frame.
- Shader hunting now guards empty visited-buffer sets before selecting the
  previous index/vertex buffer.
- `crash=1` now executes the exception handler after writing diagnostics
  instead of resuming the same faulting instruction.
- InputLayout side-car metadata is owned through D3D private `IUnknown` data
  while still returning the real `ID3D11InputLayout*` to the game.
- Cached input-layout metadata is released on `ClearState`, command-list state
  clear paths, and context teardown.

### Changed

- `Dependencies\d3dx.ini` keeps `skip_early_includes_load = 0` for EDHM/classic
  include loading.
- Audit-targeted warning cleanup for hunting buffer selection, resource-hash
  log format strings, and C++17-only syntax.
- Package version **0.1.3-alpha.1**; DLL file/product version **0.1.3**.

### Validation Needed

- Cold start to main menu.
- Supercruise to station approach/dock to recheck the old InputLayout
  location-load crash class.
- Confirm F11/theme reload or `auto_refresh_file_to_monitor` behavior.
- Confirm `debug=0` logging remains quiet.
- Keep normal play configs at `export_hlsl=0` and `dump_usage=0`.

Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
