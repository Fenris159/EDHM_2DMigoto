## EDHM_2DMigoto 0.1.4

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.4] - 2026-11-15

### Added

- Supported EDHM + EDUP chaining with the normal runtime DLL. The README links
  the recommended wiki configuration, file layout and tested scope.
- `[Logging] show_reload_message=0` hides the reload confirmation while keeping
  configuration reloads and their disk log entry. Warning notices remain
  independently controlled by the plural `show_warnings` key.
- Resource matching/view diagnostics and native runtime, proxy-chain and
  rendered-notification checks, including Windows x64/Win32 and pinned EDUP
  0.22.1 startup checks.

### Fixed

- Preserve both the outer proxy's creation-hook continuation and an explicitly
  configured downstream proxy when the EDHM DLL is renamed for chaining.
- Preserve pending settings and the last complete file after failed saves;
  retain temporarily unavailable persistent values for a reload grace period.
- Resolve resource-reference chains after parsing, retain learned pool flags,
  and keep metadata queries from allocating or refreshing live pool elements.
- Correct typed/structured `store` reads, bounded `Region(offset,size)` copies
  and views, smaller constant-buffer copies, saved SRV clears and view-cache
  invalidation. Failed reads keep the previous value.
- Preserve parser/namespace compatibility, stable `this` view reuse, profiler
  reload measurements and production binaries after mutation-check failures.

### Changed

- Reuse compatible views and configuration-section lookups, and reduce enabled
  profiler registration overhead. The dated off-game comparison report retains
  its measured scope and limits; no combined EDUP FPS gain is claimed.
- Require C++17 in ordinary and strict builds. Stabilize notification checks
  with a controlled clock boundary and retain timeout diagnostics.

### Notes

- Stable promotion of the 0.1.4 beta series. The beta sections below retain the
  detailed change history and validation checkpoints.
- With EDUP, keep its `d3d11.dll` and rename our DLL to `d3d11_edhm.dll`.
  Follow the [recommended configuration](https://github.com/Fenris159/EDHM_2DMigoto/wiki/EDUP-Compatibility-and-Configuration#recommended-configuration).
- Evidence includes native Windows/MSVC/WARP checks, off-game
  Proton 11.0/DXVK/AMD checks with EDUP 0.22.1, and an external tester's report
  about the earlier repaired build. This does not certify every GPU, AA/VR
  engine or live-game transition. Reload scheduling remains unchanged.
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
