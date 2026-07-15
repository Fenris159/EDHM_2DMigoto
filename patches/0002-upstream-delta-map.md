# EDHM delta map for upstream comparison

**Date:** 2026-07-15  
**Local branch:** `develop`  
**Comparison base:** `xxmi-base` at `af232a90fda4e5fc8799f854f07787a3bc33bcbe`

This map is for side-by-side review against `xxmi-base`. Prefer the upstream
function or ini section names below over line numbers; line numbers drift every
time `xxmi-base` advances.

## Runtime template

| Local file | Upstream context | EDHM delta |
|------------|------------------|------------|
| `Dependencies/d3dx.ini` | `[Loader]` | Target `EliteDangerous64.exe`; leave external loader disabled for normal local `d3d11.dll` installs. |
| `Dependencies/d3dx.ini` | `[Include]` | Load EDHM's shipped ini tree instead of the generic `Mods` include directory. |
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
| `DirectX11/HackerDevice.cpp` | `CreateRasterizerState1()` | Applies the same scissor-disable behavior already used by `CreateRasterizerState()`. |
| `DirectX11/ResourceHash.cpp` / `HackerContext.cpp` | Region hash cache paths | Keeps bounds and size conversions explicit where EDHM region-copy stress exposed unsafe truncation. |
| `DirectX11/globals.h` | Global state | Adds state for EDHM theme auto-refresh and the fuzzy/hash matching opt-in. |

## Review command

```powershell
git diff --word-diff=color xxmi-base -- Dependencies/d3dx.ini
git diff --function-context xxmi-base -- DirectX11/IniHandler.cpp DirectX11/ResourceHash.cpp
```
