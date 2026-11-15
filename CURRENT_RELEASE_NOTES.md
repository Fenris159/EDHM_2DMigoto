## EDHM_2DMigoto 0.1.5

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.5] - 2026-11-15

### Fixed

- Serialize reload with an in-flight settings save even when no additional
  changes are dirty. Preserve the saved values through the subsequent parse.
- Protect configuration object lifetimes across complete draw/dispatch, clear
  and input command batches. Reload defers while a batch is active and holds
  exclusive ownership through saving, parsing and initialization.
- Remove the configuration-lock wait inside borrowed persistent variables;
  serialize file transactions without adding that wait to command execution.
  Atomic variable storage allows autosave reads alongside assignments. Successful
  persistent `store` reads mark their new value for saving.
- Honor explicit depth/stencil clear selection when a hexadecimal value also
  sets the integer-UAV hint.
- Keep the original timeout and its cleanup-failure marker if writing the
  partial-output diagnostic log fails.
- Accept an already-reaped Linux child as terminated in the runner check and
  restore a meaningful smaller-CB-copy mutation after the region-path repair.

### Notes

- Follow-up to the late review of 0.1.4. Its tag and files are preserved; the
  GitHub release was reclassified as a prerelease pending these corrections.
- EDUP uses the same normal runtime DLL and the recommended wiki configuration.
  The quiet refresh option remains `[Logging] show_reload_message=0`.
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
