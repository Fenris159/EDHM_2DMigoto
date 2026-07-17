## EDHM_2DMigoto 0.1.3-alpha.5

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.3-alpha.5] - 2026-07-17

### Added

- Forwarded the documented WinRT D3D11/DXGI interop exports through the proxy,
  with safe `E_NOTIMPL` handling when the host runtime does not provide them.

### Changed

- Removed unused D3DKMT wrapper shims whose private ABI is not documented,
  while retaining the supported public D3D11 proxy surface.
- Kept EDHM runtime warnings enabled while scoping known modern-toolset warning
  suppressions to the bundled legacy DirectXTK project.

### Fixed

- Supported D3D11 device creation when callers request only an immediate context
  and balanced deferred-context references across success and failure paths.
- Released temporary COM interfaces after rejected queries and reset mirrored
  context state after runtime state-clearing operations.
- Validated DXBC section offsets and sizes before shader replacement parsing and
  retained injected depth-resource views for their full binding lifetime.
- Corrected multi-page region-hash invalidation, synchronized asynchronous frame
  analysis query tracking, and attached resource-release trackers outside cache
  locks to avoid stale results and re-entrant lock hazards.
- Avoided non-trivial thread cleanup from `DllMain` detach notifications while
  preserving process-shutdown cleanup on explicit unload.
- Made `KeyOverride` ini parsing dominance explicit instead of relying on an
  ambiguous inherited implementation.
- Corrected post-build commands so Win32 and x64 packages receive the matching
  `d3dcompiler_47.dll` architecture and removed the invalid-parameter diagnostic.

### Notes

- Package pre-release version is **0.1.3-alpha.5**; DLL file/product version
  remains **0.1.3**.
- The review PR stack intentionally leaves the vendor cache unchanged. Refresh
  it with the release workflow only after the complete stack is merged.
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
