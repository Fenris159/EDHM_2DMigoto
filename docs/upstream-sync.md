# Upstream sync workflow

This repo is independent of [XXMI-Libs-Package](https://github.com/SpectrumQT/XXMI-Libs-Package) and [3Dmigoto](https://github.com/bo3b/3Dmigoto/). Remotes exist only so you can **see** and **selectively adopt** upstream work.

## Remotes

| Name | URL | Use |
|------|-----|-----|
| `origin` | your GitHub repo | push / pull your work |
| `xxmi` | `https://github.com/SpectrumQT/XXMI-Libs-Package.git` | primary upstream for new DLL features |
| `3dmigoto` | `https://github.com/bo3b/3Dmigoto.git` | classic 3Dmigoto reference when comparing EDHM-era behavior |

Never push to `xxmi` or `3dmigoto`. Treat them as read-only.

## Fetch

```powershell
git fetch xxmi --tags
git fetch 3dmigoto master
# or
.\scripts\fetch-upstream.ps1
```

## Review

```powershell
# Commits on XXMI not in your current branch
git log --oneline HEAD..xxmi/master

# With paths of interest
git log --oneline HEAD..xxmi/master -- DirectX11/ Injector/

# Full patches for a single commit
git show <sha>
```

Tags from XXMI (e.g. `v0.9.4`) are available after `git fetch xxmi --tags`:

```powershell
git tag -l "v*"
git log --oneline v0.9.3..v0.9.4
```

## Integrate selectively

### Cherry-pick (preferred)

```powershell
git switch develop
git cherry-pick <sha>
# resolve conflicts if needed, then:
git cherry-pick --continue
```

If a commit does not apply cleanly, prefer a manual port of the change over force-fitting a large merge.

### Merge a small range (when history is linear and low-risk)

```powershell
git switch develop
git merge --no-ff <sha-or-tag>
```

Avoid blind merges of all of `xxmi/master` once you have diverged with EDHM patches; review first.

## Compare with classic 3Dmigoto

When investigating “EDHM worked on old 3Dmigoto but not XXMI”:

```powershell
git fetch 3dmigoto master
git diff 3dmigoto/master xxmi/master -- DirectX11/IniHandler.cpp
# or against your tree
git diff 3dmigoto/master HEAD -- DirectX11/
```

## After integrating

1. Build the affected projects in Visual Studio.
2. Smoke-test against EDHM / Elite Dangerous.
3. Note the upstream SHA(s) in `patches/` or the PR description.
4. Merge `develop` → `main` only when stable.
