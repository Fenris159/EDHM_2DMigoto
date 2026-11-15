## EDHM_2DMigoto 0.1.4-beta.3

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.4-beta.3] - 2026-11-15

### Added

- `[Logging] show_reload_message=0` hides the completed config-reload
  confirmation while retaining reloads, disk logging and independent warning
  controls. The default remains enabled.
- Product-DLL notification checks on x64 and Win32 inspect the rendered
  confirmation and warning colors, read back reloaded GPU constants, and
  require the completion entry in the disk log. A pinned beta.2 control must
  fail the suppression cases on visible confirmation pixels.

### Changed

- Make notification checks independent of real clock progress and retain partial
  stdout/stderr with a failed receipt when a fixture times out.

### Notes

- Set `show_reload_message=0` in the root `d3dx.ini`. To hide warning notices
  as well, use the separate, plural `show_warnings=0` key.
- The option changes only the confirmation text. It leaves reload timing,
  theme-file auto-refresh and EDUP chaining active. The specific cause of the
  tester's transition-time reloads remains unconfirmed without their log.
- Validation includes native Windows/MSVC/WARP checks and an off-game
  Proton 11.0/DXVK/AMD check beneath EDUP 0.22.1. It does not establish
  installed-game transition timing, AA/VR output or FPS gains.
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
