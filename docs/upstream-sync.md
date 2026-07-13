# Upstream sync workflow

This repo is independent of [XXMI-Libs-Package](https://github.com/SpectrumQT/XXMI-Libs-Package) and [3Dmigoto](https://github.com/bo3b/3Dmigoto/). Remotes exist only so you can **see** and **selectively adopt** upstream work.

## Branch model

| Branch | Purpose | What belongs here |
|--------|---------|-------------------|
| `xxmi-base` | Clean XXMI import | Only fast-forwards / merges from `xxmi/master`. No EDHM patches. |
| `develop` | Integration + EDHM work | Cherry-picks from `xxmi-base`, EDHM compatibility fixes, docs that help day-to-day work. |
| `main` | EDHM release | Stable snapshots of `develop` ready to ship for EDHM. |

```text
  xxmi (remote)
       │
       │  git fetch / merge --ff-only
       ▼
  xxmi-base          ◄── keep current with SpectrumQT XXMI
       │
       │  cherry-pick / careful port (not blind full merge once diverged)
       ▼
  develop            ◄── your daily branch
       │
       │  merge when stable
       ▼
  main               ◄── EDHM compatibility releases
```

Do **not** commit EDHM-only changes on `xxmi-base`. That branch should stay a readable mirror of upstream so `git log develop..xxmi-base` stays meaningful.

## Remotes

| Name | URL | Use |
|------|-----|-----|
| `origin` | your GitHub repo | push / pull your branches (`main`, `develop`, `xxmi-base`) |
| `xxmi` | `https://github.com/SpectrumQT/XXMI-Libs-Package.git` | source of truth for advancing `xxmi-base` |
| `3dmigoto` | `https://github.com/bo3b/3Dmigoto.git` | classic 3Dmigoto reference for EDHM-era behavior |

Never push to `xxmi` or `3dmigoto`. Treat them as read-only (`push` URL is `no_push` locally).

## Keep `xxmi-base` current

### From GitHub (recommended for hands-off updates)

Workflow: [`.github/workflows/update-xxmi-base.yml`](../.github/workflows/update-xxmi-base.yml)

| Trigger | When |
|---------|------|
| Schedule | Mondays and Thursdays at 12:00 UTC |
| Manual | **Actions → Update xxmi-base → Run workflow** |

The job fetches [SpectrumQT/XXMI-Libs-Package](https://github.com/SpectrumQT/XXMI-Libs-Package) `master` and fast-forwards `xxmi-base` on this repo. It does **not** touch `develop` or `main`.

- If already current, the job exits cleanly.
- If `xxmi-base` has non-upstream commits, the job fails (protects the mirror). Re-run with **force** only if you mean to reset the branch to pure XXMI.

Scheduled workflows only run from the repo **default branch** (`main`), so keep this workflow file on `main`.

### From local CLI

```powershell
.\scripts\fetch-upstream.ps1 -UpdateBase -PushBase -ShowNew
```

Manual equivalent:

```powershell
git fetch xxmi --tags
git fetch . xxmi/master:xxmi-base   # ff-only; does not check out the branch
git push origin xxmi-base
```

Use fast-forward only so `xxmi-base` never gains unrelated local commits by accident. If that fails, someone committed on `xxmi-base` who should not have — fix that before continuing.

## Review what is new for develop

```powershell
# Commits on the clean base not yet in develop
git log --oneline develop..xxmi-base

# Paths that matter most for EDHM
git log --oneline develop..xxmi-base -- DirectX11/ Injector/

# Inspect one commit
git show <sha>
```

Tags from XXMI (e.g. `v0.9.4`) after `git fetch xxmi --tags`:

```powershell
git tag -l "v*"
git log --oneline v0.9.3..v0.9.4
```

## Integrate into develop (selective)

### Cherry-pick (preferred)

```powershell
git switch develop
git cherry-pick <sha>
# resolve conflicts if needed, then:
git cherry-pick --continue
```

If a commit does not apply cleanly, prefer a manual port of the change over force-fitting a large merge.

### Merge a small range (only when low-risk)

```powershell
git switch develop
git merge --no-ff <sha-or-tag>
```

Avoid blind merges of all of `xxmi-base` once `develop` has diverged with EDHM patches; review first.

## Release to main

```powershell
git switch main
git merge --no-ff develop -m "Release: EDHM-compatible build notes here"
git push origin main
```

Tag releases on `main` if useful (`vEDHM-0.1.0`, etc.).

## Compare with classic 3Dmigoto

When investigating “EDHM worked on old 3Dmigoto but not XXMI”:

```powershell
git fetch 3dmigoto master
git diff 3dmigoto/master xxmi-base -- DirectX11/IniHandler.cpp
git diff 3dmigoto/master develop -- DirectX11/
```

## After integrating

1. Build the affected projects in Visual Studio.
2. Smoke-test against EDHM / Elite Dangerous.
3. Note the upstream SHA(s) in `patches/` or the PR description.
4. Merge `develop` → `main` only when stable.
