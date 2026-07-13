# EDHM compatibility bridge (XXMI → classic 3Dmigoto behaviour)

**Date:** 2026-07-12  
**Branch:** `develop`  
**Reference mod:** [psychicEgg/EDHM](https://github.com/psychicEgg/EDHM) Odyssey package (ships `d3d11.dll` **3Dmigoto 1.4.5** + MS `d3dcompiler_47.dll`)  
**Upstream base:** SpectrumQT XXMI-Libs-Package (`xxmi-base`)  
**Classic reference:** [bo3b/3Dmigoto](https://github.com/bo3b/3Dmigoto)

## Symptoms addressed

With pure XXMI DLLs under an unmodified EDHM tree:

- Some HUD shaders appear to “miss” (wrong/default colours, elements not recolored)
- Texture / render-target driven selection (`filter_index`) can pick the wrong override

EDHM does **not** need code changes; the DLL must honour classic 3Dmigoto semantics that EDHM’s `d3dx.ini` + `EDHM-ini/*` + `ShaderFixes/*` were authored against.

## EDHM dependency surface (v22)

| Mechanism | EDHM usage |
|-----------|------------|
| `ShaderFixes\*.txt` | Assembly pixel/vertex replacements (`ini_params = 120`) |
| `[Include]` / `include_recursive` | All theme + `35aac.ini` TextureOverrides |
| Fuzzy `TextureOverride` | RT size rules (`match_width = < height * …`, `match_priority`, `filter_index`) |
| Hash `TextureOverride` | IB/VB/texture hashes + `match_index_count` → `filter_index` |
| `checktextureoverride` | Several ShaderOverrides |
| `auto_refresh_file_to_monitor` | Theme signal file (present in EDHM’s shipped DLL string table) |

`d3dcompiler_47.dll` is the Microsoft redistributable (not built from this tree).

## Gaps found (XXMI vs classic / EDHM DLL)

### 1. `skip_early_includes_load` defaulted to **true** (XXMI)

XXMI defers `[Include]` parsing until a delayed `ReloadConfig`. Classic 3Dmigoto always loads includes on first parse.

EDHM puts **hundreds** of TextureOverrides and Constants under `EDHM-ini/` via includes. Until reload, filter rules and colours are incomplete → looks like “missed shaders/textures”.

**Fix:** default `skip_early_includes_load` to **false**. XXMI launcher can still set it true in its own ini.

### 2. Fuzzy TextureOverrides after hash matches (XXMI `e3b66a44`)

Classic: if any **hash** TextureOverride matches, fuzzy matches are **not** considered.

XXMI: always also ran fuzzy matches. `filter_index` is taken from the **highest priority** match in the combined list. A hashed texture that also matches a fuzzy RT size rule can get the **wrong** `filter_index`, breaking EDHM colour logic.

**Fix:** restore classic early-return when hash matches exist. Opt-in XXMI dual-match with:

```ini
[Rendering]
fuzzy_match_alongside_hash = 1
```

### 3. `auto_refresh_file_to_monitor` missing

EDHM’s `d3dx.ini` and its shipped 1.4.5 DLL support:

```ini
[System]
auto_refresh_file_to_monitor=EDHM-ini/ThemeSettings.json
```

Stock bo3b / XXMI source did not implement this key. EDHM_UI uses it as a **signal** to re-read config when themes change.

**Fix:** watch file mtime each Present; on change, set `gReloadConfigPending`.

### 4. Unsafe `HackerInputLayout` cast

XXMI wraps input layouts and used `static_cast<HackerInputLayout*>` on every `IASetInputLayout`. Foreign layouts → UB / wrong orig pointer.

**Fix:** mark originals with private data; unwrap via `HackerInputLayout::FromLayout`.

### 5. Version identity

`version.h` still said 1.3.16 / “3Dmigoto”. EDHM ships 1.4.5. Updated product identity to **EDHM_2DMigoto 1.4.5** for clearer comparison (not a claim of binary identity with bo3b 1.4.5).

## Files touched

- `DirectX11/ResourceHash.cpp` — classic hash-vs-fuzzy precedence  
- `DirectX11/IniHandler.cpp` — include default, auto_refresh parse, fuzzy opt-in  
- `DirectX11/HackerDXGI.cpp` — auto_refresh mtime check  
- `DirectX11/HackerInputLayout.*` / `HackerContext.cpp` — safe unwrap  
- `DirectX11/globals.h` — new state  
- `version.h` — product/version  

## Test plan

1. Build `EDHM_2DMigoto.sln` **Release | x64** → `d3d11.dll`  
2. Copy into an EDHM install **replacing only** `d3d11.dll` (keep EDHM’s `d3dcompiler_47.dll`, inis, ShaderFixes)  
3. Confirm HUD recolors match stock EDHM 1.4.5 DLL  
4. Change theme via EDHM_UI / touch `ThemeSettings.json` → config reloads without F11  
5. Optional: enable `fuzzy_match_alongside_hash=1` only if testing AGMG-style dual-match  

## Not changed (by design)

- Full stereo pipeline (EDHM uses `stereo_params = -1`)  
- XXMI resource pools / region-hash features (opt-in; EDHM does not enable `track_region_hashes`)  
- Slim develop tree vs full `xxmi-base`  
