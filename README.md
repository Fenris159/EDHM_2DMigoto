# EDHM_2DMigoto

[![CI](https://github.com/Fenris159/EDHM_2DMigoto/actions/workflows/ci.yml/badge.svg)](https://github.com/Fenris159/EDHM_2DMigoto/actions/workflows/ci.yml)
[![Release](https://github.com/Fenris159/EDHM_2DMigoto/actions/workflows/release.yml/badge.svg)](https://github.com/Fenris159/EDHM_2DMigoto/actions/workflows/release.yml)
[![Update xxmi-base](https://github.com/Fenris159/EDHM_2DMigoto/actions/workflows/update-xxmi-base.yml/badge.svg)](https://github.com/Fenris159/EDHM_2DMigoto/actions/workflows/update-xxmi-base.yml)
[![GitHub release](https://img.shields.io/github/v/release/Fenris159/EDHM_2DMigoto?include_prereleases&sort=semver)](https://github.com/Fenris159/EDHM_2DMigoto/releases)
[![GitHub downloads](https://img.shields.io/github/downloads/Fenris159/EDHM_2DMigoto/total)](https://github.com/Fenris159/EDHM_2DMigoto/releases)
[![GitHub stars](https://img.shields.io/github/stars/Fenris159/EDHM_2DMigoto?style=social)](https://github.com/Fenris159/EDHM_2DMigoto/stargazers)
[![GitHub issues](https://img.shields.io/github/issues/Fenris159/EDHM_2DMigoto)](https://github.com/Fenris159/EDHM_2DMigoto/issues)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE.GPL.txt)
[![Language](https://img.shields.io/github/languages/top/Fenris159/EDHM_2DMigoto)](https://github.com/Fenris159/EDHM_2DMigoto)
[![Languages count](https://img.shields.io/github/languages/count/Fenris159/EDHM_2DMigoto)](https://github.com/Fenris159/EDHM_2DMigoto)
[![Built for EDHM](https://img.shields.io/badge/Built%20for-EDHM-1f6feb)](https://github.com/psychicEgg/EDHM)
[![2Dmigoto](https://img.shields.io/badge/Runtime-2Dmigoto%20(XXMI)-6f42c1)](https://github.com/SpectrumQT/XXMI-Libs-Package)
[![Platform](https://img.shields.io/badge/Platform-Windows%20x64-0078d4)](docs/building.md)
[![Linux](https://img.shields.io/badge/Linux-Proton%20%2F%20Wine-FCC624?logo=linux&logoColor=black)](docs/building.md)
[![MSBuild](https://img.shields.io/badge/Build-VS2022%20%7C%20MSVC-5c2d91)](docs/building.md)
[![main vs develop](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FFenris159%2FEDHM_2DMigoto%2Fbadges%2F.github%2Fbadges%2Fmain-vs-develop.json)](https://github.com/Fenris159/EDHM_2DMigoto/compare/develop...main)
[![develop vs xxmi-base](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FFenris159%2FEDHM_2DMigoto%2Fbadges%2F.github%2Fbadges%2Fdevelop-vs-xxmi-base.json)](https://github.com/Fenris159/EDHM_2DMigoto/compare/xxmi-base...develop)

Independent **XXMI / 2Dmigoto** libraries tailored for **[EDHM](https://github.com/psychicEgg/EDHM)** (Elite Dangerous HUD Mod) workflows.

## Documentation (start here)

**Developer documentation lives on the [project Wiki](https://github.com/Fenris159/EDHM_2DMigoto/wiki):**

| Topic | Wiki page |
|-------|-----------|
| Overview & map | [Home](https://github.com/Fenris159/EDHM_2DMigoto/wiki) |
| Install / use with EDHM | [Getting Started](https://github.com/Fenris159/EDHM_2DMigoto/wiki/Getting-Started) |
| Build from source | [Building](https://github.com/Fenris159/EDHM_2DMigoto/wiki/Building) |
| Shader hunting | [Shader Hunting](https://github.com/Fenris159/EDHM_2DMigoto/wiki/Shader-Hunting) |
| Advanced dumps (`export_hlsl=3`, …) | [Advanced Hunting & Export](https://github.com/Fenris159/EDHM_2DMigoto/wiki/Advanced-Hunting-and-Export) *(optional)* |
| Config / overrides | [Configuration Reference](https://github.com/Fenris159/EDHM_2DMigoto/wiki/Configuration-Reference), [ShaderFixes & Overrides](https://github.com/Fenris159/EDHM_2DMigoto/wiki/ShaderFixes-and-Overrides) |
| Releases & vendor cache | [Releasing & CI](https://github.com/Fenris159/EDHM_2DMigoto/wiki/Releasing-and-CI), [Vendor Cache](https://github.com/Fenris159/EDHM_2DMigoto/wiki/Vendor-Cache-and-Releases) |

In-tree `docs/` remains a short reference; the **wiki is canonical** for development how-tos.

This repository starts from [SpectrumQT/XXMI-Libs-Package](https://github.com/SpectrumQT/XXMI-Libs-Package) (itself a streamlined fork of [bo3b/3Dmigoto](https://github.com/bo3b/3Dmigoto/)) but is **not** a GitHub fork of either project. You own this tree completely; useful upstream fixes can still be pulled in selectively.

## Goal products (EDHM)

| Artifact | Source |
|----------|--------|
| **`d3d11.dll`** | Built from the **DirectX11** project (`EDHM_2DMigoto.sln`) — published via [Releases](https://github.com/Fenris159/EDHM_2DMigoto/releases) |
| **`d3dcompiler_47.dll`** | **Not built here** — Microsoft redistributable (Windows SDK / EDHM package / `Dependencies/`) |

Everything else from full XXMI (loaders, other D3DCompiler proxies, DXGI shim, unused DirectXTK platforms, …) stays on **`xxmi-base`** only. `develop` keeps the **minimum build graph** for EDHM’s DLL.

## Why this exists

EDHM historically relied on classic 3Dmigoto-era runtime behavior. XXMI (often called “2Dmigoto”) modernizes and strips stereo features, which can break EDHM expectations. This repo is the place to:

- Keep a **focused**, buildable XXMI-derived tree under your control
- Apply EDHM-specific patches without fighting an upstream fork relationship
- Cherry-pick only the XXMI / 3Dmigoto changes that help EDHM from `xxmi-base`
- Ship versioned **`d3d11.dll`** builds for EDHM / EDHM_UI to consume

## Credits

| Project | Authors / maintainers |
|--------|------------------------|
| [3Dmigoto](https://github.com/bo3b/3Dmigoto/) | Chiri, [Bo3b](https://github.com/bo3b), [DarkStarSword](https://github.com/DarkStarSword), and contributors |
| [XXMI-Libs-Package](https://github.com/SpectrumQT/XXMI-Libs-Package) | [SpectrumQT](https://github.com/SpectrumQT) and the [AGMG Community](https://discord.gg/agmg) |
| [EDHM](https://github.com/psychicEgg/EDHM) | psychicEgg, Fred89210, and the EDHM community |
| Deviare-InProcess | Nektra (GPLv3; used by the inherited 3Dmigoto codebase) |

See `AUTHORS.txt` and `COPYING.txt` for fuller attribution.

## License

Library source in this repository is released under the **GNU General Public License version 3**. See `LICENSE.GPL.txt` and `COPYING.txt`.

Game-specific shaders, configs, and mod assets are **not** automatically covered by that license; they remain owned by their respective authors.

## Branches

| Branch | Role |
|--------|------|
| `main` | **Stable releases** (after first merge from develop) |
| `develop` | Day-to-day work + **pre-release** builds |
| `xxmi-base` | Clean full XXMI mirror (no EDHM patches) |

```text
xxmi/master  ──ff──►  xxmi-base  ──selective port──►  develop  ──release──►  main
                      (full XXMI)                      (slim EDHM)          (ship)
```

**Ahead / behind badges** (Shields endpoint; JSON lives on the dedicated [`badges`](https://github.com/Fenris159/EDHM_2DMigoto/tree/badges) branch so refreshes never spam `main` history):

| Badge | Meaning |
|-------|---------|
| `main vs develop` | How `main` sits vs `develop` (ahead = release-only commits; behind = unreleased develop work) |
| `develop vs xxmi-base` | How `develop` sits vs the XXMI mirror (ahead = EDHM/local work; behind = XXMI commits not yet ported) |

Click a badge for the GitHub compare view. Counts are approximate history divergence (not a formal fork relationship). Refreshed daily (or after pushes to `main`) by [Branch status badges](.github/workflows/branch-status-badges.yml).

## Releases and CI

| Workflow | Purpose |
|----------|---------|
| [**CI**](.github/workflows/ci.yml) | On push/PR to **`main` only** (e.g. develop→main merge) + manual: meta checks + MSBuild |
| [**Release**](.github/workflows/release.yml) | Manual: build from `main` (release) or `develop` (pre-release), optional SemVer tag, publish assets |
| [**Update xxmi-base**](.github/workflows/update-xxmi-base.yml) | Keep the XXMI mirror current; open/refresh an `upstream-xxmi` review issue when it advances |
| [**Branch status badges**](.github/workflows/branch-status-badges.yml) | Daily / after `main` push: refresh Shields JSON on the **`badges`** branch only |

**Versioning:** see root `VERSION`, `CHANGELOG.md`, and `CURRENT_RELEASE_NOTES.md` (used as GitHub Release body). Details: [`docs/releasing.md`](docs/releasing.md).

| Channel | Tag example | GitHub Release |
|---------|-------------|----------------|
| `main` | `v0.1.2` | Latest stable release |
| `develop` | `v0.1.3-alpha.1` | Alpha release candidate |

```text
Actions → Release → Run workflow
  branch: main | develop
  version_action: use-current | bump-patch | bump-minor | bump-major | bump-prerelease
  create_tag: true
  update_vendor_cache: true   # also commits vendor/edhm-runtime/d3d11.dll (LFS)
```

After a successful non-draft release, `vendor/edhm-runtime/` holds the same `d3d11.dll` as the Release asset (convenience mirror for clones/packaging). Prefer pinning by **Release tag** for production EDHM packaging.

## Repository layout (`develop`)

### Foreground

| Path | Purpose |
|------|---------|
| `EDHM_2DMigoto.sln` | Focused solution: DirectX11 + required static libs |
| `DirectX11/` | **Product** — builds `d3d11.dll` |
| `Dependencies/` | Runtime package pieces (`d3dx.ini`, bundled `d3dcompiler_47.dll`) |
| `VERSION` / `CHANGELOG.md` / `CURRENT_RELEASE_NOTES.md` | SemVer package versioning and release notes |
| `vendor/edhm-runtime/` | Mirrored `d3d11.dll` + checksums after each Release (Git LFS); see folder README |
| `patches/` | EDHM-specific patch notes |
| `docs/` | Build, layout, upstream-sync, releasing |
| `scripts/` | Upstream fetch, version helpers, vendor cache update |
| `dist/` | Local build outputs (gitignored by default) |

### Background (required to link `d3d11.dll`)

`BinaryDecompiler/`, `D3D_Shaders/`, `HLSLDecompiler/`, `DirectXTK/` (slim), `Nektra/`, `pcre2/`, `crc32c-hw-1.0.5/`, shared root helpers and fonts.

See `docs/layout.md`.

## Building

1. **Visual Studio 2022** (MSVC v143 / Windows 10 SDK)
2. Open **`EDHM_2DMigoto.sln`**
3. Build **DirectX11** | **Release | x64** (or **Zip Release | x64**)
4. Outputs under `x64\Release\` or `x64\Zip Release\`
5. Optional: copy into `dist/` for local packaging

See `docs/building.md`.

## Tracking upstream

```powershell
.\scripts\fetch-upstream.ps1 -UpdateBase -PushBase -ShowNew
git log --oneline develop..origin/xxmi-base -- DirectX11/
git switch develop
git cherry-pick <sha>
```

From GitHub: **Actions → Update xxmi-base → Run workflow**. When the mirror advances, an issue labeled `upstream-xxmi` lists commits not yet in `develop`. Details: `docs/upstream-sync.md`.

## Status

- `xxmi-base` — full XXMI mirror (background)
- `develop` — slim tree + EDHM compatibility work + pre-release pipeline
- `main` — stable channel (scaffold until first release merge from `develop`)
