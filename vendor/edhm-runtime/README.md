# vendor/edhm-runtime

**Mirrored runtime cache for EDHM consumers.** Updated automatically by the
[Release](../../.github/workflows/release.yml) workflow after a successful package.

| File | Purpose |
|------|---------|
| `VERSION` | Package SemVer (no leading `v`), e.g. `0.1.0` or `0.1.0-beta.1` |
| `SOURCE_TAG` | Git tag that produced this cache, e.g. `v0.1.0-beta.1` |
| `CHANNEL` | `release` (from `main`) or `prerelease` (from `develop`) |
| `d3d11.dll` | Built DLL (Git LFS) |
| `d3d11.dll.sha256` | SHA-256 of `d3d11.dll` |
| `RELEASE_URL` | GitHub Release page for this tag (when published) |

## Do not hand-edit

Do not manually replace these files. Cut a release instead:

**Actions → Release → Run workflow**

Source of truth for distribution remains **GitHub Releases** assets. This folder is a convenience mirror for cloning, packaging scripts, and EDHM sync tools.

## Using from EDHM

1. Prefer downloading `d3d11.dll` from the matching [Release](https://github.com/Fenris159/EDHM_2DMigoto/releases) (pin by tag).
2. Or copy `vendor/edhm-runtime/d3d11.dll` after verifying `d3d11.dll.sha256`.
3. Keep EDHM’s own `d3dcompiler_47.dll` (Microsoft redistributable) unless you intentionally package the MS DLL separately.

## LFS

`*.dll` under this directory is stored with **Git LFS**. Clone with LFS installed:

```powershell
git lfs install
git clone https://github.com/Fenris159/EDHM_2DMigoto.git
```
