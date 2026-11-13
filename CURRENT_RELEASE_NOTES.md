## EDHM_2DMigoto 0.1.4-beta.1

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.4-beta.1] - 2026-11-13

### Fixed

- Reject empty expression groups and accept leading-dot float literals.
- Preserve pending settings, locale and the last complete settings file after a
  failed save; stop a reload that cannot save or wipe its settings.
- Retain temporarily unavailable persistent values for one reload, with optional
  indefinite retention through `clear_unknown_settings=0`; discard values after
  persistence is removed.
- Resolve custom-resource reference flags after all command lists are parsed,
  including constant-buffer aliases and incompatible cycles. Read pool metadata
  without allocating elements or refreshing their lifetime.
- Route pool proxy resets correctly and preserve namespaced, Unicode and
  hyphenated pool-variable identifiers.
- Read complete structured elements for `store`, validate buffer types and
  indices, preserve failed-read values, and release owned source views.
- Interpret `Region(offset,size)` as a byte count, validate backing bounds,
  create bounded views, and distinguish copied CBs from aligned CB references.
- Rebuild a referenced view when its requested offset, extent, format, stride,
  target kind or raw-view mode changes on the same resource.
- Clear stale structured stride during CB recreation and guard unsupported
  typed-buffer formats against division by zero.

- Enable the required C++17 language mode in ordinary builds as well as strict builds.
- Preserve smaller explicit CB copies, clear saved SRV-backed resources through
  writable views, retain runtime-learned pool flags and scalar-variable precedence,
  and keep valid warm views for `this` targets.
- Retain reload initialization measurements until a completed frame can be reported.
- Read nested pool indexes without changing pool state during metadata queries,
  retain bind-flag learning for initially empty references, and skip unused
  destination metadata for existing resource caches.
- Match normal lazy-slot, FIFO and same-frame expiration behavior in detached
  metadata previews, and preserve resource identities when a copy is accepted.
- Preserve hot resources and cached spatial data during preview adoption, and
  retain statically pinned resource targets after pool proxy changes.
- Share provisional resources across aliases of one root slot and reuse the
  resolved destination and logical key throughout a copy, including index-read
  expiration and same-pool source lookups.
- Restore production binaries after failed mutation checks and exclude unverified
  executables from failure artifacts.

### Added

- Summary-mode CPU timing and counts for resource matching, fuzzy candidates,
  view creation and view reuse; document how to capture the performance monitor.

### Changed

- Reuse the current INI section while parsing. In the isolated EDHM fixture,
  median configuration loading fell from 73.971 ms to 71.625 ms under Wine;
  this is a CPU load-time measurement. Constants compaction and static-condition
  memoization were evaluated and deferred because useful gains were not established.
- Register profiler command identities once per collection window, reducing
  enabled-monitor overhead in the native interpreter fixture.
- Reuse compatible source views after checking resource, interface and active
  descriptor fields; retain factory fallback for regions, overrides and planar
  video. Preserve existing pipeline bindings when required view creation fails.
- Add native runtime regression checks, deliberate-break verification and
  optional strict Zip Release validation for both Windows architectures.

### Notes

- Development beta; installed Elite Dangerous + EDHM smoke testing and game
  frame-time validation remain pending. See [the implementation and validation
  report](https://github.com/Fenris159/EDHM_2DMigoto/blob/develop/docs/next-release-implementation.md).
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
