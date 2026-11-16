## EDHM_2DMigoto 0.1.6

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.6] - 2026-11-15

### Fixed

- Prevent quit-time crashes when EDUP or another D3D owner releases tracked
  resources after the runtime's global resource registry has been destroyed.
  Retire registry callbacks before global-member destruction and at DLL detach;
  late final releases avoid the retired registry and its lock while normal
  resource cleanup continues during play. Trackers use the Windows process heap
  so late callbacks also avoid the already-terminated C runtime allocator.

### Added

- Shutdown regression checks for live metadata cleanup, final release after
  actual registry destruction, final private-data callbacks across standalone
  and chained DLL teardown, official EDUP 0.22.2, and ordinary unload after
  object release. These use off-game child processes and WARP.
- Matching PDB and map files for optimized release builds, with a checksummed
  symbols ZIP containing the exact DLL and provenance for later crash analysis.
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
