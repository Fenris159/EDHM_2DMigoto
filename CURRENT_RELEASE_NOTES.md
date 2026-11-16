## EDHM_2DMigoto 0.1.8

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.
## [0.1.8] - 2026-11-16

### Fixed

- Restore EDHM frame actions when EDUP replaces the game's D3D11 Present for
  XeSS Frame Generation: hotkeys, presets, transitions, `[Present]` commands,
  post commands and configuration reload keep running on the established game
  presentation thread. Observe the outer proxy's actual Present boundary;
  generated frames and repeated buffer reads do not advance EDHM's frame state.
  Normal presentation resumes without duplicate actions. Native D3D12 helper
  swap chains retain the 0.1.7 pass-through fix. Thanks to
  [Duceowen's follow-up report and proposed recovery patch](https://github.com/Fenris159/EDHM_2DMigoto/issues/52#issuecomment-6908828546).

### Added

- Product-DLL regression probes for a third-party Present replacement, including
  zero-draw frames, hotkeys, timed preset transitions, reload, failure cleanup,
  foreign threads, secondary chains and swap-chain recreation. Both Present
  APIs are exercised off-game; the published 0.1.7 DLL is a starvation control.
  These checks do not establish in-game XeSS output or FPS gains.
---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
