## EDHM_2DMigoto 0.1.4-beta.2

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.4-beta.2] - 2026-11-15

### Fixed

- Keep installed device-creation hook continuations separate from selected
  backend exports, so renamed chain loading preserves both an outer proxy such
  as EDUP and an explicitly configured downstream proxy.

### Added

- Product-DLL proxy-chain checks for both device-creation APIs, early and lazy
  initialization, repeated calls, combined outer/downstream proxies and
  missing/self/invalid proxy fallback on x64 and Win32. Invalid-proxy checks
  require a matching architecture and the missing-export rejection diagnostic.
- A pinned EDUP 0.22.1 before/after startup check: the old DLL selects the chain
  while leaving EDUP's device and Present path inactive; the repaired DLL
  activates both and registers its F8 menu binding.
- Separate regression controls for outer-proxy and downstream-proxy bypass.
  Failed or incomplete rejection evidence remains failed in the JSON receipts.

### Changed

- Scope Actions read permissions to the workflow jobs that consume them.
- Document rebuilding the product DLL before running proxy-chain checks.

### Notes

- An external tester reported an earlier repaired test build working in-game
  with EDUP and EDHM. Native Windows/MSVC/WARP checks of the final routing fix
  remain separate from that report and do not establish Elite AA or VR output.
- When pairing with EDUP, retain EDUP's `d3d11.dll` and install this release's
  DLL as `d3d11_edhm.dll`. Set `[advanced] real_dll=d3d11_edhm.dll` in EDUP's
  configuration and `[System] load_library_redirect=0` in our `d3dx.ini`. See
  [the configuration guide](https://github.com/Fenris159/EDHM_2DMigoto/blob/develop/docs/ed-unofficial-patch-compatibility.md#configuration-i-recommend-for-this-pairing).
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
