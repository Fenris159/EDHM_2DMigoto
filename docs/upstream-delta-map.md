# Upstream Delta Map

**Date:** 2026-07-16 (updated 2026-07-21)

**Local branch:** `linux-compatibility` (target: `develop`) — *integrated*: the review stack has been merged into `develop`/`main`; this document is retained for provenance.

**Comparison base:** `xxmi-base` at `af232a90fda4e5fc8799f854f07787a3bc33bcbe`

This map is for side-by-side review against `xxmi-base`. Prefer the upstream
function or ini section names below over line numbers; line numbers drift every
time `xxmi-base` advances.

## Runtime template

| Local file | Upstream context | EDHM delta |
|------------|------------------|------------|
| `Dependencies/d3dx.ini` | `[Loader]` | Target `EliteDangerous64.exe`; leave external loader disabled for normal local `d3d11.dll` installs. |
| `Dependencies/d3dx.ini` | `[Include]` | Load EDHM's shipped ini tree instead of the generic `Mods` directory while retaining upstream `DISABLED*` and `desktop.ini` recursive exclusions. |
| `Dependencies/d3dx.ini` | `[Constants]` | Keep upstream examples; EDHM live HUD state belongs in included EDHM ini files. |
| `Dependencies/d3dx.ini` | `[System] skip_early_includes_load` | Default to `0` so included TextureOverrides are available before first frame, matching classic 3Dmigoto behavior. |
| `Dependencies/d3dx.ini` | `[System] auto_refresh_file_to_monitor` | EDHM_UI touches `EDHM-ini/ThemeSettings.json`; runtime schedules `ReloadConfig` when it changes. |
| `Dependencies/d3dx.ini` | `[Device] hide_cursor` | Keep `1` for EDHM software/themed cursor setups. |
| `Dependencies/d3dx.ini` | `[Rendering] fuzzy_match_alongside_hash` | Default off so hash TextureOverrides win before fuzzy resource matching, matching classic 3Dmigoto. |
| `Dependencies/d3dx.ini` | `[Rendering] stereo_params` | Keep disabled; EDHM shader replacements use `ini_params`, not the XXMI stereo path. |
| `Dependencies/d3dx.ini` | `[Logging]` | Split `enabled` from legacy `calls`, add bounded log rotation, and keep crash logging optional. |

## Code deltas

| Local file | Upstream context | EDHM delta |
|------------|------------------|------------|
| `DirectX11/IniHandler.cpp` | `LoadConfigFile()` / `[Logging]` | `enabled=1` opens `d3d11_log.txt` without forcing API-call spam; `calls=1` remains the legacy verbose path. |
| `DirectX11/IniHandler.cpp` | `LoadConfigFile()` / `[Include]` | `skip_early_includes_load` defaults false for EDHM/classic 3Dmigoto startup semantics. |
| `DirectX11/IniHandler.cpp` | `LoadConfigFile()` / `[System]` | Parses `auto_refresh_file_to_monitor` as a DLL-relative path for EDHM theme reloads. |
| `DirectX11/IniHandler.cpp` | `LoadConfigFile()` / `[Rendering]` | Parses `fuzzy_match_alongside_hash` as an opt-in for XXMI dual hash+fuzzy matching. |
| `DirectX11/IniHandler.cpp` | `LoadConfigFile()` / `[Rendering]` | Parses `stereo_params` with EDHM default `-1`, so no StereoParams declaration is emitted unless requested. |
| `DirectX11/ResourceHash.cpp` | `find_texture_overrides_for_resource()` | Restores classic early return after hash matches unless `fuzzy_match_alongside_hash=1`. |
| `DirectX11/HackerDXGI.cpp` | Present path | Polls `auto_refresh_file_to_monitor` and schedules config reload after EDHM_UI theme changes. |
| `DirectX11/HackerInputLayout.*` | Input-layout wrapper | Uses a side-car cache attached to the real D3D input layout; the game receives and binds the real layout pointer. |
| `DirectX11/HackerContext.cpp` | `IASetInputLayout()` / `IAGetInputLayout()` | Tracks the side-car for frame analysis, but passes through and returns real D3D layout pointers. |
| `DirectX11/HackerDevice.cpp` | `CreateInputLayout()` | Creates the side-car cache without replacing `*ppInputLayout`, avoiding real D3D calls with wrapper pointers. |
| `DirectX11/HackerDevice.cpp` | Rasterizer state creation | Applies scissor-disable behavior to both descriptor versions through local copies, without modifying the game's `const` descriptors. |
| `DirectX11/ResourceHash.*` / `HackerContext.cpp` | Region hash cache paths | Keeps bounds and size conversions explicit, guarantees missing-key probes terminate, and avoids invalidation truncation. |
| `DirectX11/globals.h` | Global and TLS state | Adds EDHM theme/fuzzy settings and per-thread D3D11 redirect suppression. |
| `DirectX11/WineCompat.*` | *(new)* | Wine/Proton detection, `wine_compat` profile, and host/Flatpak compatibility reporting. |
| `DirectX11/IniHandler.cpp` | Ini/include and path handling | Parses `wine_compat`, converts ini values and glob patterns as UTF-8, bounds DLL-relative paths, and reports inaccessible includes or shader directories without aborting the game. |
| `DirectX11/D3D11Wrapper.cpp` / `DLLMainHook.cpp` | D3D11 initialization and chaining | Thread-safe initialization, validated standard chaining, guarded optional exports, initialized COM outputs, caller-safe feature-level forcing, and thread-local one-call redirect suppression. |
| `DirectX11/FrameAnalysis.cpp` / `HackerDevice.cpp` / `Hunting.cpp` | Optional analysis and shader reload paths | Bounds filename and source scans, initializes cache/reload outputs, validates timestamps and file sizes, and avoids null diagnostic outputs. |
| `DirectX11/DirectX11.vcxproj` and static-library projects | Release compiler settings | Enables `/GS` stack-cookie protection while retaining the existing XXMI project structure and optimization settings. |
| `Dependencies/d3dx.ini` | `[System]` | Documents `wine_compat` and Wine/Proton load notes. |

## Review command

```powershell
git diff --word-diff=color xxmi-base -- Dependencies/d3dx.ini
git diff --function-context xxmi-base -- DirectX11/IniHandler.cpp DirectX11/ResourceHash.cpp
```
