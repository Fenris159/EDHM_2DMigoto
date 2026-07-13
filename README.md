# EDHM_2DMigoto

Independent **XXMI / 2Dmigoto** libraries tailored for **EDHM** (Elite Dangerous HUD Mod) workflows.

This repository starts from [SpectrumQT/XXMI-Libs-Package](https://github.com/SpectrumQT/XXMI-Libs-Package) (itself a streamlined fork of [bo3b/3Dmigoto](https://github.com/bo3b/3Dmigoto/)) but is **not** a GitHub fork of either project. You own this tree completely; useful upstream fixes can still be pulled in selectively.

## Goal products (EDHM)

| Artifact | Source |
|----------|--------|
| **`d3d11.dll`** | Built from the **DirectX11** project (`EDHM_2DMigoto.sln`) |
| **`d3dcompiler_47.dll`** | **Not built here** — Microsoft redistributable copied from the Windows SDK at build time, and also shipped under `Dependencies/` for packaging |

Everything else from full XXMI (loaders, other D3DCompiler proxies, DXGI shim, 7zip tools, pcre2 sources, unused DirectXTK platforms, …) stays on **`xxmi-base`** only. `develop` / release trees keep the **minimum build graph** for those two DLLs.

## Why this exists

EDHM historically relied on classic 3Dmigoto-era runtime behavior. XXMI (often called “2Dmigoto”) modernizes and strips stereo features, which can break EDHM expectations. This repo is the place to:

- Keep a **focused**, buildable XXMI-derived tree under your control
- Apply EDHM-specific patches without fighting an upstream fork relationship
- Cherry-pick only the XXMI / 3Dmigoto changes that help EDHM from `xxmi-base`

## Credits

| Project | Authors / maintainers |
|--------|------------------------|
| [3Dmigoto](https://github.com/bo3b/3Dmigoto/) | Chiri, [Bo3b](https://github.com/bo3b), [DarkStarSword](https://github.com/DarkStarSword), and contributors |
| [XXMI-Libs-Package](https://github.com/SpectrumQT/XXMI-Libs-Package) | [SpectrumQT](https://github.com/SpectrumQT) and the [AGMG Community](https://discord.gg/agmg) |
| Deviare-InProcess | Nektra (GPLv3; used by the inherited 3Dmigoto codebase) |

See `AUTHORS.txt` and `COPYING.txt` for fuller attribution.

## License

Library source in this repository is released under the **GNU General Public License version 3**. See `LICENSE.GPL.txt` and `COPYING.txt`.

Game-specific shaders, configs, and mod assets are **not** automatically covered by that license; they remain owned by their respective authors.

## Branches

| Branch | Role |
|--------|------|
| `main` | **EDHM compatibility releases** — starts scaffold-only; receives merges from `develop` when ready to ship |
| `develop` | Slim working tree + EDHM patches for `d3d11.dll` |
| `xxmi-base` | **Full clean XXMI mirror** — background / cherry-pick source only |

```text
xxmi/master  ──ff──►  xxmi-base  ──selective port──►  develop  ──release──►  main
                      (full XXMI)                      (slim EDHM)          (ship)
```

## Repository layout (`develop`)

### Foreground — what you work on

| Path | Purpose |
|------|---------|
| `EDHM_2DMigoto.sln` | Focused solution: DirectX11 + required static libs |
| `DirectX11/` | **Product** — builds `d3d11.dll` |
| `Dependencies/` | Runtime package pieces (`d3dx.ini`, bundled `d3dcompiler_47.dll`) |
| `patches/` | EDHM-specific patch notes |
| `docs/` | Build, layout, upstream-sync |
| `scripts/` | Helper scripts |
| `dist/` | Local build outputs (gitignored by default) |

### Background — required to build `d3d11.dll` (normally don’t edit unless porting)

| Path | Role |
|------|------|
| `BinaryDecompiler/` | Static lib (shader binary decode) |
| `D3D_Shaders/` | Assembler / signature sources compiled into DirectX11 |
| `HLSLDecompiler/` | HLSL decompile helper used by DirectX11 |
| `DirectXTK/` | Slimmed Toolkit (Desktop 2017 project only) |
| `Nektra/` | Prebuilt hook libs |
| `pcre2/` | Prebuilt PCRE2 libs + header |
| `crc32c-hw-1.0.5/` | CRC sources |
| Root `util.*`, `log.h`, `version.h`, … | Shared 3Dmigoto/XXMI helpers |
| `*.spritefont` | Overlay fonts embedded by DirectX11 |

Full XXMI (Injector, other D3DCompiler wrappers, etc.) lives on **`xxmi-base`**, not on `develop`.

See `docs/layout.md` for keep/drop detail.

## Building

1. **Visual Studio 2022** (MSVC v143 / Windows 10 SDK)
2. Open **`EDHM_2DMigoto.sln`**
3. Build **DirectX11** | **Release | x64** (or **Zip Release | x64**)
4. Outputs: `d3d11.dll` under the configuration output folder; post-build may copy SDK `d3dcompiler_47.dll` beside it
5. Optional: copy both DLLs + your EDHM `d3dx.ini` into `dist/` for testing

See `docs/building.md`.

## Tracking upstream

```powershell
.\scripts\fetch-upstream.ps1 -UpdateBase -PushBase -ShowNew
git log --oneline develop..origin/xxmi-base -- DirectX11/
git switch develop
git cherry-pick <sha>   # or port manually into the slim tree
```

From GitHub: **Actions → Update xxmi-base → Run workflow**.

Details: `docs/upstream-sync.md`.

## Status

- `xxmi-base` � full XXMI mirror (background)
- `develop` � slimmed tree for building `d3d11.dll` (+ packaging `d3dcompiler_47.dll`)
- `main` � **scaffold only** (licenses, docs, scripts, xxmi-base updater). Merge `develop` here for the first EDHM release.