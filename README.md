# EDHM_2DMigoto

Independent **XXMI / 2Dmigoto** library tree tailored for **[EDHM](https://github.com/bluelettr/EDHM)** (Elite Dangerous HUD Mod) workflows.

This repository starts from [SpectrumQT/XXMI-Libs-Package](https://github.com/SpectrumQT/XXMI-Libs-Package) (itself a streamlined fork of [bo3b/3Dmigoto](https://github.com/bo3b/3Dmigoto/)) but is **not** a GitHub fork of either project. You own this tree completely; useful upstream fixes can still be pulled in selectively.

## Why this exists

EDHM historically relied on classic 3Dmigoto-era runtime behavior. XXMI (often called “2Dmigoto”) modernizes and strips stereo features, which is great for many games but can break EDHM expectations. This repo is the place to:

- Keep a clean, buildable XXMI-based codebase under your control
- Apply EDHM-specific patches without fighting an upstream fork relationship
- Cherry-pick only the XXMI / 3Dmigoto changes that help EDHM

## Credits

Original and upstream work — thank you:

| Project | Authors / maintainers |
|--------|------------------------|
| [3Dmigoto](https://github.com/bo3b/3Dmigoto/) | Chiri, [Bo3b](https://github.com/bo3b), [DarkStarSword](https://github.com/DarkStarSword), and contributors |
| [XXMI-Libs-Package](https://github.com/SpectrumQT/XXMI-Libs-Package) | [SpectrumQT](https://github.com/SpectrumQT) and the [AGMG Community](https://discord.gg/agmg) |
| Deviare-InProcess | Nektra (GPLv3; used by the inherited 3Dmigoto codebase) |

See `AUTHORS.txt` and `COPYING.txt` for fuller attribution.

## License

Library source in this repository is released under the **GNU General Public License version 3**. See `LICENSE.GPL.txt` and `COPYING.txt`.

Game-specific shaders, configs, and mod assets are **not** automatically covered by that license; they remain owned by their respective authors.

## Repository layout

| Path | Purpose |
|------|---------|
| `DirectX11/`, `Injector/`, … | XXMI / 2Dmigoto source (imported base) |
| `patches/` | EDHM-specific patch notes, diffs, and design notes |
| `docs/` | Build notes, upstream-sync workflow, EDHM compatibility notes |
| `scripts/` | Helper scripts (fetch upstream, package builds, etc.) |
| `dist/` | Built DLLs / release packages (artifacts ignored by default) |

## Branches

| Branch | Role |
|--------|------|
| `main` | **EDHM compatibility releases** — stable code you ship / package |
| `develop` | Day-to-day EDHM patches and integration work |
| `xxmi-base` | **Clean XXMI import mirror** — kept current with upstream XXMI only |

Flow:

```text
xxmi/master  ──fetch──►  xxmi-base  ──cherry-pick / selective port──►  develop  ──release──►  main
```

- Advance `xxmi-base` when SpectrumQT ships useful XXMI commits (no EDHM edits on that branch).
- On `develop`, pick only what you need, then layer EDHM fixes.
- When `develop` is stable for EDHM, merge into `main` for a release.

## Building

Same general requirements as XXMI / 3Dmigoto:

1. **Visual Studio 2022** (or compatible MSVC toolset)
2. Open `StereovisionHacks.sln` (historical solution name from 3Dmigoto/XXMI)
3. Build the DirectX11 / injector targets you need
4. Copy outputs into `dist/` when packaging for EDHM testing

See `docs/building.md` for more detail as it is filled in.

## Tracking upstream without depending on it

Remotes are configured for **fetch-only awareness** (you push only to `origin`):

```text
origin     https://github.com/Fenris159/EDHM_2DMigoto.git
xxmi       https://github.com/SpectrumQT/XXMI-Libs-Package.git
3dmigoto   https://github.com/bo3b/3Dmigoto.git
```

Typical update workflow:

```powershell
# 1. Fetch + fast-forward the clean XXMI mirror branch
.\scripts\fetch-upstream.ps1 -UpdateBase -ShowNew

# 2. Review what is new on xxmi-base vs develop
git log --oneline develop..xxmi-base
git log -p --reverse develop..xxmi-base -- DirectX11/

# 3. On develop, selectively integrate then test
git switch develop
git cherry-pick <commit>

# 4. When ready to release EDHM-compatible builds
git switch main
git merge --no-ff develop
```

More detail: `docs/upstream-sync.md`.

Helper script (local):

```powershell
.\scripts\fetch-upstream.ps1
.\scripts\fetch-upstream.ps1 -UpdateBase -PushBase -ShowNew
```

From GitHub: **Actions → Update xxmi-base → Run workflow** (also runs Mon/Thu on a schedule). See `docs/upstream-sync.md`.

## How this differs from stock XXMI

| Stock XXMI | This repository |
|------------|-----------------|
| Oriented toward XXMI Launcher / AGMG game stacks | Oriented toward **EDHM / Elite Dangerous** |
| GitHub fork of 3Dmigoto lineage | **Independent** repo; remotes only for tracking |
| Upstream-driven feature set | Selective integration + EDHM compatibility patches |

## Status

Initial import of XXMI-Libs-Package history is complete. `xxmi-base` tracks clean upstream XXMI. EDHM-specific runtime patches are developed on `develop` and released via `main`.
