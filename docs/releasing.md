# Releasing EDHM_2DMigoto

## Version numbers

| Location | Purpose |
|----------|---------|
| **`VERSION`** | Package / GitHub Release SemVer (`0.1.0`, `0.1.0-beta.1`) |
| **`version.h`** | DLL *file* properties (may stay `1.4.5` for EDHM tooling familiarity) |
| **Tags** | `v0.1.0` (release) · `v0.1.0-beta.1` / `v0.1.0-rc.1` (pre-release) |

Follow [Semantic Versioning](https://semver.org/) and [Keep a Changelog](https://keepachangelog.com/).

## Files

| File | Role |
|------|------|
| `CHANGELOG.md` | Full history (`## [Unreleased]`, `## [X.Y.Z] - date`, …) |
| `CURRENT_RELEASE_NOTES.md` | Body used by the Release workflow for GitHub Release notes |
| `scripts/version.ps1` | Bump VERSION, compute tags, sync notes from CHANGELOG |

Before a cut, update **CHANGELOG** (move Unreleased items into a version section), then either:

```powershell
.\scripts\version.ps1 -Action sync-notes
```

…or let the Release workflow run `sync-notes` for you.

## Channels

| Branch | Workflow result | Tag style |
|--------|-----------------|-----------|
| **`main`** | **Release** (stable) | `vMAJOR.MINOR.PATCH` |
| **`develop`** | **Pre-release** | `vMAJOR.MINOR.PATCH-beta.N` (or `-rc.N` / `-alpha.N`) |

## Manual release (GitHub UI)

1. Ensure the branch has `EDHM_2DMigoto.sln` (not scaffold-only `main`).
2. **Actions → Release → Run workflow**
3. Choose:
   - **branch**: `main` or `develop`
   - **version_action**: `use-current` or a bump
   - **pre_label**: `beta` / `rc` / `alpha` (develop)
   - **create_tag**: usually `true`
   - **update_vendor_cache**: usually `true` (skips on draft)
4. Workflow will:
   - Optionally bump `VERSION`
   - Refresh `CURRENT_RELEASE_NOTES.md` from `CHANGELOG.md`
   - Commit + push if needed
   - Create annotated tag
   - Build **Zip Release | x64**
   - Publish GitHub Release with `d3d11.dll`, zip, and SHA256 files
   - Commit **`vendor/edhm-runtime/`** (DLL via Git LFS + metadata) on the same branch

## CI

**Actions → CI** runs on every push/PR to `main` and `develop`:

- Meta checks (`VERSION`, changelog, release notes)
- Windows MSBuild **Release | x64** when the solution exists
- Uploads `d3d11.dll` as a workflow artifact (14 days)

Scaffold-only `main` skips compile but still runs meta checks.

## Vendor cache (`vendor/edhm-runtime`)

After a **non-draft** Release succeeds, the same workflow (when `update_vendor_cache` is true, default) commits:

| Path | Contents |
|------|----------|
| `vendor/edhm-runtime/d3d11.dll` | Built DLL (**Git LFS**) |
| `vendor/edhm-runtime/d3d11.dll.sha256` | Checksum |
| `vendor/edhm-runtime/VERSION` | SemVer without `v` |
| `vendor/edhm-runtime/SOURCE_TAG` | e.g. `v0.1.0-beta.1` |
| `vendor/edhm-runtime/CHANNEL` | `release` or `prerelease` |
| `vendor/edhm-runtime/RELEASE_URL` | GitHub Release URL |

- Pre-releases update the cache on **`develop`**
- Stable releases update the cache on **`main`**
- GitHub **Release assets** remain the distribution source of truth; vendor is a convenience mirror

Local helper (after a manual build):

```powershell
.\scripts\update-vendor-cache.ps1 `
  -DllPath 'x64\Zip Release\d3d11.dll' `
  -Version 0.1.0-beta.1 `
  -Tag v0.1.0-beta.1 `
  -Channel prerelease
```

## Local helpers

```powershell
.\scripts\version.ps1 -Action get
.\scripts\version.ps1 -Action bump-patch
.\scripts\version.ps1 -Action bump-prerelease -PreLabel beta
.\scripts\version.ps1 -Action tag -Channel release
.\scripts\version.ps1 -Action tag -Channel prerelease -PreLabel beta
.\scripts\version.ps1 -Action sync-notes
```
