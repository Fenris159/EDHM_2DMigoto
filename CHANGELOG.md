# Changelog

All notable changes to **EDHM_2DMigoto** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Package / release tags use this file and the root `VERSION` file
(`vMAJOR.MINOR.PATCH` for releases, `vMAJOR.MINOR.PATCH-alpha.N` /
`-beta.N` / `-rc.N` for pre-releases).

DLL file properties (`version.h`) match package SemVer (same MAJOR.MINOR.PATCH as
`VERSION`). Behaviour still targets classic 3Dmigoto / EDHM compatibility.

## [Unreleased]

### Fixed

- Automatically skip pixel draws and compute dispatches that read unsupported
  typed UAV formats, including command-list submissions. Keep the crash guard
  active with fixes disabled and log device support and rejected shader creation.

### Planned

- Further EDHM smoke validation against stock 3Dmigoto / EDHM packages

## [0.1.9] - 2026-11-22

### Fixed

- Preserve requested D3D11.1+ and D3D12.x feature levels during
  `D3D11CreateDevice` and `D3D11CreateDeviceAndSwapChain` instead of
  downgrading them to `11.0`, while filtering down-level (`< 11.0`) fallback
  entries in modes `0` and `2` and stepping down unsupported newer enums on
  `E_INVALIDARG` in mode `2` without skipping supported intermediate levels.
- Default `allow_platform_update` to enabled so `ID3D11Device1`,
  `ID3D11DeviceContext1`, and `IDXGIFactory2` remain available out of the box;
  preserve modern `IDXGIFactory4+` interfaces in `Hooked_CreateDXGIFactory2`
  and pass through `IDXGISwapChain4` for HDR10 metadata and upscaler/frame-gen
  consumers.
- Complete `ID3D11Device1` and `ID3D11DeviceContext1` hook/trampoline coverage,
  synchronize hooked wrapper map removal on release, complete lazy
  `GetImmediateContext1` resource binding and `[Constants]` initialization,
  and preserve up to 64 UAV slots on FL 11.1+ devices in OM state save/restore
  and frame-analysis UAV dumps.
- Disable `LoadLibraryExW` redirection from `DllMain` onward when loaded under
  a chained module name (such as `d3d11_edhm.dll`) so system/SDK probes for
  `System32\d3d11.dll` are never redirected back to an outer proxy.
- Scope forwarded-Present suppression to its swap-chain wrapper, allowing an
  independent primary frame to progress while another device has an in-flight
  secondary proxy call.

### Added

- Windows WARP regression coverage for D3D11.1+/12.0 feature-level
  preservation, mixed/down-level fallback filtering, mode `2` `E_INVALIDARG`
  step-down, `ID3D11Device1`/`ID3D11DeviceContext1` hooked and lazy-first
  `[Constants]` initialization, renamed-DLL `LoadLibraryExW` pass-through, and
  64-slot FL 11.1 OM UAV state save/restore.

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

## [0.1.7] - 2026-11-15

### Fixed

- Pass D3D12 command-queue swap chains through all four DXGI creation APIs
  without D3D11 overrides or wrappers. This preserves `IDXGISwapChain4` for
  EDUP's XeSS Frame Generation helper and avoids the false DirectX 12 warning
  and extra D3D11On12 device. D3D11 game swap chains retain EDHM behavior.
  Null devices are also passed to native DXGI validation rather than the
  legacy device lookup, preventing an access violation on invalid calls.
  Thanks to [Duceowen for the report and patch](https://github.com/Fenris159/EDHM_2DMigoto/issues/52).

### Added

- Off-game D3D12 WARP checks for native swap-chain interfaces and dimensions,
  unchanged creation failures, warning-free helper initialization, and D3D11
  resource tracking after helper calls. The 0.1.6 code reproduces the reported
  `0x80004002` interface failure on x64 and Win32.

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

## [0.1.5] - 2026-11-15

### Added

- EDUP updater distribution: `d3d11_edhm.dll`, its checksum and
  `edup-runtime.json` are published with each release. The manifest provides a
  version-pinned URL, size, hash and release metadata.
- `vendor/edup-runtime/` mirrors the renamed DLL and manifest alongside the
  normal EDHM cache; both binaries use the same product build.

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
- Read saved settings before deferred include initialization; preserve changes
  made during an autosave instead of clearing their pending state.
- Resolve resource-reference chains after parsing, retain learned pool flags,
  and keep metadata queries from allocating or refreshing live pool elements.
- Correct typed/structured `store` reads, bounded `Region(offset,size)` copies
  and views, smaller constant-buffer copies, saved SRV clears and view-cache
  invalidation. Failed reads keep the previous value.
- Copy vertex-buffer byte regions independently of the binding stride, honor
  explicit integer clears through retained RTVs, and share detached metadata
  between STATIC pool-variable pins and dynamic aliases.
- Preserve parser/namespace compatibility, stable `this` view reuse, profiler
  reload measurements and production binaries after mutation-check failures.

### Changed

- Reuse compatible views and configuration-section lookups, and reduce enabled
  profiler registration overhead. The dated off-game comparison report retains
  its measured scope and limits; no combined EDUP FPS gain is claimed.
- Require C++17 in ordinary and strict builds. Stabilize notification checks
  with a controlled clock boundary and retain timeout diagnostics.
- Stop timed-out mutation-build descendants before restoring source, rebuild
  clean objects, and recognize lowercase Wine WoW64 library directories.

### Notes

- Stable promotion of the 0.1.4 beta series. The beta sections below retain the
  detailed change history and validation checkpoints.
- With EDUP, keep its `d3d11.dll` and rename our DLL to `d3d11_edhm.dll`.
  Follow the [recommended configuration](https://github.com/Fenris159/EDHM_2DMigoto/wiki/EDUP-Compatibility-and-Configuration#recommended-configuration).
- Evidence includes native Windows/MSVC/WARP checks, off-game
  Proton 11.0/DXVK/AMD checks with EDUP 0.22.1, and an external tester's report
  about the earlier repaired build. This does not certify every GPU, AA/VR
  engine or live-game transition. Reload scheduling remains unchanged.

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

## [0.1.3] - 2026-09-21

### Added

- Optional two-level shader context hunting: keep a selected pixel or vertex
  shader and cycle the distinct draw contexts that use it, then mark a
  conservative `TextureOverride` candidate plus its parent `ShaderOverride`.
  Disabled by default (`advanced_hunting=0`); see
  `docs/advanced-shader-hunting.md`.

### Changed

- Promoted the 0.1.3-alpha train to a stable package tag. DLL file/product
  version remains **0.1.3**.
- Hardened hunting resource-state updates so setter-triggered refreshes run
  only while a context-hunting session is active; the initial snapshot still
  runs before the session starts.
- Continued native quality modernization and SonarCloud history
  reconciliation on the maintained DX11/EDHM paths.

### Fixed

- Context hunting reports now switch back to decimal after writing the hex
  resource hash, so `match_first_*` values stay numeric when `index_count` is 0.
- Release `sync-notes` no longer fails when `CHANGELOG.md` contains blank
  lines (PowerShell 7 rejected empty strings on a mandatory `[string[]]`).

### Notes

- Latest previous package tag was **0.1.3-alpha.6**. This release is the
  stable 0.1.3 cut of that train plus context hunting.
- Hunting Level 2 is for shader authors. Normal play configs can leave
  `advanced_hunting=0`.

## [0.1.3-alpha.6] - 2026-08-12

### Added

- Ported XXMI 0.9.5–0.9.8 command-list and resource-pool updates: pool
  variables and spatial indexing, `CommandArgumentReader`, `store` rewrite,
  vertex-layout element format/offset commands, and expression additions
  (`frame_number`, `draw_number`, `dispatch_number`, binary literals, unary
  functions, static evaluation).
- Added `docs/ini-command-reference.md` covering the command-list language,
  including the newly ported XXMI commands.
- Added Qodo and SonarCloud pull-request check workflows for `main` and
  `xxmi-base`.

### Changed

- Restored the compiled DLL product name and copyright metadata to the
  upstream 3Dmigoto values.
- Clarified that EDHM_2DMigoto is the repository/build-package name while the
  runtime remains 3Dmigoto-derived software based on XXMI / 2Dmigoto.
- Reworded DLL diagnostics to identify the wrapper as 3Dmigoto and EDHM as its
  compatibility profile and configuration target.
- Addressed first-party SonarCloud blockers and high-risk smells (`nullptr`,
  safer string helpers, compiled shader blobs excluded from PHP scans).

### Fixed

- Applied input-layout overrides on every draw with correct AddRef/Release
  ownership, and restored deferred overrides even when the override already
  matched the current layout.
- Kept `persistent_variables` in sync when pool metadata copies a `PERSIST`
  flag, and stopped treating `wcstof` underflow (for example `1e-50`) as
  infinity.
- Included all four spatial-hash inputs in the L2 cache key, rejected
  overflowing region-size math, and freed the temporary readback scratch
  buffer after the CPU snapshot is copied.
- Made `random()`’s shared call counter atomic for deferred-context use.

### Notes

- Package pre-release version is **0.1.3-alpha.6**; DLL file/product version
  remains **0.1.3**.
- Elite still receives real `ID3D11InputLayout*` pointers. Layout override
  commands bind `GetOrigInputLayout()` from the side-car cache; XXMI’s COM
  wrapper return path is not used.
- Present timing remains `GetTickCount64()`.

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
- Skipped all non-trivial cleanup from `DllMain` during process termination
  (`DLL_PROCESS_DETACH` with `lpvReserved` set); per-thread TLS cleanup on
  thread detach and full shutdown on explicit dynamic unload remain by design.
- Made `KeyOverride` ini parsing dominance explicit instead of relying on an
  ambiguous inherited implementation.
- Corrected post-build commands so Win32 and x64 packages receive the matching
  `d3dcompiler_47.dll` architecture and removed the invalid-parameter diagnostic.

### Notes

- Package pre-release version is **0.1.3-alpha.5**; DLL file/product version
  remains **0.1.3**.
- The review PR stack intentionally leaves the vendor cache unchanged. Refresh
  it with the release workflow only after the complete stack is merged.

## [0.1.3-alpha.4] - 2026-07-16

### Added

- Added Wine/Proton host detection and a startup compatibility report containing
  effective System settings, DLL paths, DLL overrides, conflicting Proton
  renderer options, and Flatpak environment detection.
- Added standard Flatpak Steam setup instructions for both physical EDHM
  directories and narrowly permitted EDHM-UI symlink targets.

### Changed

- Wine compatibility now uses the standard Proton/Wine-prefix D3D11 chain with
  no renamed local DXVK DLLs or `proxy_d3d11` configuration.
- The automatic Wine profile is limited to `load_library_redirect=0` and
  `check_foreground_window=0`; initialization delays remain explicit opt-in
  troubleshooting settings.
- Moved remaining successful shader and Texture3D creation messages behind
  `debug=1` and made recursive include patterns and ini values UTF-8 safe.
- Re-aligned `Dependencies/d3dx.ini` with the `xxmi-base` section order and
  examples, restored standard recursive exclusions, and added concise guidance
  for choosing EDHM, Windows, Wine/Proton, proxy, hashing, and logging settings.
- Enabled compiler stack-cookie protection for all Release and Zip Release
  runtime, decompiler, and bundled DirectXTK configurations.
- Added the Linux compatibility guide to repository navigation and release ZIPs.

### Fixed

- Reject invalid chained D3D11 modules before their missing exports can be
  called, guard optional D3D11 exports, and serialize one-time initialization.
- Made one-call D3D11 redirect suppression thread-local and initialized its TLS
  storage before installing hooks.
- Prevented recursive include scanning from reading before short filenames and
  added explicit diagnostics for inaccessible include and ShaderFixes paths.
- Stopped scissor overrides from modifying caller-owned `const` rasterizer
  descriptors in either D3D11 rasterizer-state creation path.
- Prevented a full one-slot region hash table from looping forever on a missing
  key, bounded probing, and removed end-offset truncation during invalidation.
- Fixed a debug-layer null dereference and released debug interfaces after use.
- Stopped device creation from modifying the game's feature-level array and
  initialized optional device and swap-chain outputs before forwarding calls.
- Hardened frame analysis, shader cache and live reload paths against null
  metadata, malformed files, unbounded first-line scans, and invalid timestamps.
- Fixed optional shader decompiler error output using a null file and treating a
  C runtime `FILE*` as a Win32 handle.
- Avoided a zero-count UAV state query in the overlay path and validated
  embedded font resources without leaking device/context references on failure.
- Hardened DLL-relative path construction, blank `user_config` handling,
  profile-helper paths, concurrent crash-exception counting, COM output handling,
  and warning-prone numeric conversions.

### Notes

- Package pre-release version is **0.1.3-alpha.4**; DLL file/product version
  remains **0.1.3**.
- The bundled vendor cache remains pinned to the last published release DLL
  until the release workflow publishes `v0.1.3-alpha.4` and refreshes it.

## [0.1.3-alpha.3] - 2026-07-15

### Changed

- Streamlined `Dependencies/d3dx.ini` around EDHM and Elite Dangerous runtime
  use, replacing stale XXMI-loader and non-EDHM game comments with EDHM-focused
  guidance.
- Aligned stable sample config defaults with the installed Odyssey layout,
  including the standard include tree, active theme auto-refresh example,
  logging defaults, cursor handling, profile defaults, and shader override
  examples.
- Added `stereo_params` parsing support for decompiled shader declarations and
  hardened `auto_refresh_file_to_monitor` path handling for EDHM theme reloads.

### Fixed

- Corrected non-fatal exception filtering so continuable diagnostic exceptions
  are ignored consistently.
- Suppressed inherited DirectXTK C5045 warning noise in the bundled DirectXTK
  project file.

### Notes

- Includes all **0.1.3-alpha.2** DX11 hardening. The bundled vendor cache
  remains pinned to the last published release DLL until the release workflow
  publishes `v0.1.3-alpha.3` and refreshes `vendor/edhm-runtime/`.

## [0.1.3-alpha.2] - 2026-07-14

### Changed

- DX11 type-safety / resource-management hardening (context, device, resource
  hash, ini paths) on top of the alpha.1 audit fixes.
- Package pre-release version is **0.1.3-alpha.2**; DLL file/product version
  remains **0.1.3**.

### Notes

- Includes all **0.1.3-alpha.1** fixes (Present1 logging, hunting empty-set
  guards, crash-handler resume, InputLayout `SetPrivateDataInterface` ownership,
  `skip_early_includes_load=0` sample, warning cleanup).
- Alpha train for live Elite smoke testing. Prefer Release DLL; keep
  `export_hlsl=0` and `dump_usage=0` for normal play.

## [0.1.3-alpha.1] - 2026-07-14

### Fixed

- `IDXGISwapChain1::Present1` no longer force-enables global debug logging
  every frame; `Present1` now follows the normal `debug=1` path.
- Shader hunting no longer dereferences the previous visited index/vertex
  buffer when the visited-buffer set is empty.
- Crash handler level `crash=1` now executes the exception handler after
  writing diagnostics instead of resuming the same faulting instruction.
- `HackerInputLayout` side-cars are now owned through D3D private
  `IUnknown` data (`SetPrivateDataInterface`) instead of a raw pointer blob.
  The game still receives the real `ID3D11InputLayout*`.
- Context-side cached input-layout metadata is released on `ClearState`,
  command-list state clear paths, and context teardown.

### Changed

- Develop template `Dependencies/d3dx.ini` keeps
  `skip_early_includes_load = 0` so EDHM includes are available during DLL
  initialization like classic 3Dmigoto.
- Cleaned audit-targeted build warnings around hunting buffer selection,
  resource-hash logging format strings, and the C++17-only `if constexpr`
  helper.
- Package pre-release version is **0.1.3-alpha.1**; DLL file/product version
  is **0.1.3**.

### Notes

- Alpha release candidate for live Elite smoke testing. Validate cold start,
  station approach/dock, F11/theme reload, and quiet `debug=0` logging before
  promoting this train.
- Keep normal play configs at `export_hlsl=0` and `dump_usage=0`.

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

## [0.1.1] - 2026-07-13

### Fixed

- **Crash on location load** (ACCESS_VIOLATION in `System32\d3d11.dll`): stop
  returning XXMI’s `HackerInputLayout` COM wrapper to the game. Layouts are
  cached as a side-car on the real D3D object (private data); the game always
  receives and binds genuine `ID3D11InputLayout` pointers. Side-car no longer
  `Release()`s the game’s Create ref (double-free risk).
- `CreateRasterizerState1` now honours `rasterizer_disable_scissor` the same way
  as `CreateRasterizerState` (Elite may use the DESC1 path).
- Release workflow: detect existing git tags via exit code / `ls-remote` instead
  of broken PowerShell `if (git rev-parse …)` (false “tag already exists”).

### Changed

- Frame analysis resolves layout cache via `FromLayout` after `IAGetInputLayout`
  (compatible with real layout pointers).
- **`version.h` file/product version aligned with package SemVer** (`0.1.1`)
  instead of classic 3Dmigoto `1.4.5` labeling.

### Notes

- EDHM with large IniParams sets (300+ float4 slots) still works; the optional
  on-screen “exceeds recommended 256” notice is a soft warning and is suppressed
  when `show_warnings=0`.
- Recommended play config with software/EDHM cursor paths: `hide_cursor=1`.
- Carrier management letterboxing/border of 3D world is present with stock
  3Dmigoto as well (not treated as a regression).

## [0.1.0] - 2026-07-13

### Added

- Independent repository based on SpectrumQT XXMI-Libs-Package (2Dmigoto), not a GitHub fork
- Tracking remotes for `xxmi` and `3dmigoto`; `xxmi-base` mirror branch + update Action
- Slim `develop` tree focused on building `d3d11.dll` for EDHM
- EDHM compatibility bridge (classic include load, hash TextureOverride precedence,
  `auto_refresh_file_to_monitor`, safer InputLayout unwrap)
- Scaffold `main` for release merges; docs for build, layout, upstream sync
- Release / pre-release GitHub Actions, CI build, changelog, and release notes pipeline
- `vendor/edhm-runtime/` cache updated automatically by the Release workflow (Git LFS for `d3d11.dll`)

### Changed

- Product name in `version.h`: **EDHM_2DMigoto** (file version was still labeled
  1.4.5 for tooling familiarity; package SemVer is `VERSION`)

### Notes

- `d3dcompiler_47.dll` remains the Microsoft redistributable (SDK / EDHM package)
- Stereo features remain stripped (2Dmigoto); XXMI performance-oriented code paths retained

[Unreleased]: https://github.com/Fenris159/EDHM_2DMigoto/compare/v0.1.4...HEAD
[0.1.4]: https://github.com/Fenris159/EDHM_2DMigoto/compare/v0.1.3...v0.1.4
[0.1.4-beta.3]: https://github.com/Fenris159/EDHM_2DMigoto/compare/v0.1.4-beta.2...v0.1.4-beta.3
[0.1.4-beta.2]: https://github.com/Fenris159/EDHM_2DMigoto/compare/v0.1.4-beta.1...v0.1.4-beta.2
[0.1.4-beta.1]: https://github.com/Fenris159/EDHM_2DMigoto/compare/v0.1.3...aa68c0ccc51b088740f6eecaba1f3582cc507f04
[0.1.3]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.3
[0.1.3-alpha.6]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.3-alpha.6
[0.1.3-alpha.5]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.3-alpha.5
[0.1.3-alpha.4]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.3-alpha.4
[0.1.3-alpha.3]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.3-alpha.3
[0.1.3-alpha.2]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.3-alpha.2
[0.1.3-alpha.1]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.3-alpha.1
[0.1.2]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.2
[0.1.1]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.1
[0.1.0]: https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/v0.1.0
