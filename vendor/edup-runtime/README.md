# EDUP runtime downloads

This folder mirrors the published x64 EDHM runtime as **`d3d11_edhm.dll`** for
EDUP installers and updaters. It contains the same bytes as the release's
`d3d11.dll`, with the filename EDUP expects for its EDHM chain target.

## Updater URLs

Use the [latest stable manifest](https://github.com/Fenris159/EDHM_2DMigoto/releases/latest/download/edup-runtime.json)
as the update source. Every release publishes the manifest and these assets:

| Asset | Permanent latest-stable URL |
| --- | --- |
| Renamed runtime | [d3d11_edhm.dll](https://github.com/Fenris159/EDHM_2DMigoto/releases/latest/download/d3d11_edhm.dll) |
| Checksum | [d3d11_edhm.dll.sha256](https://github.com/Fenris159/EDHM_2DMigoto/releases/latest/download/d3d11_edhm.dll.sha256) |
| Version and pinned download | [edup-runtime.json](https://github.com/Fenris159/EDHM_2DMigoto/releases/latest/download/edup-runtime.json) |

Read the manifest first, compare `version`, then download its `download_url`
and verify `sha256` and `size_bytes`. That URL is pinned to `source_tag`, so
publishing another release between the two requests cannot mix versions.
Install the result as `file_name` (`d3d11_edhm.dll`). EDUP keeps its own
`d3d11.dll`.

The manifest also supplies `schema_version` (currently `1`), `channel`
(`release` or `prerelease`), `architecture` (`x64`), `checksum_url`,
`release_url` and `source_url`. GitHub's latest-stable route excludes
prereleases. Explicit tag URLs can be used for testing or rollback.

## Repository mirror

| File | Contents |
| --- | --- |
| `d3d11_edhm.dll` | Exact released DLL, stored with Git LFS |
| `d3d11_edhm.dll.sha256` | SHA-256 and destination filename |
| `manifest.json` | Same data as the release's `edup-runtime.json` |
| `VERSION`, `SOURCE_TAG`, `CHANNEL`, `RELEASE_URL` | Plain-text release metadata |

On `main`, this mirror follows stable releases through the protected-branch
cache PR. Release assets are published before that PR and are the preferred
HTTP source. A plain raw Git URL for the DLL returns an LFS pointer; use the
manifest's release URL or clone with Git LFS.

The [configuration guide](https://github.com/Fenris159/EDHM_2DMigoto/wiki/EDUP-Compatibility-and-Configuration#recommended-configuration)
covers `real_dll=d3d11_edhm.dll`, EDHM assets, and quiet refresh notifications.
Keep the project's [license and attribution notices](../../THIRD_PARTY_NOTICES.md)
with redistributed packages; the normal release ZIP includes them.

The release workflow builds both filenames from one product DLL.
`scripts/update-vendor-cache.ps1` updates this folder and `vendor/edhm-runtime`
together. Follow [the release procedure](../../docs/releasing.md) when updating
the mirror.
