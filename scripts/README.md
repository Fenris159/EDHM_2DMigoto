# Scripts

| Script | Description |
|--------|-------------|
| `fetch-upstream.ps1` | Fetch `xxmi` / `3dmigoto`; optionally fast-forward `xxmi-base` |
| `version.ps1` | Read/bump `VERSION`, compute SemVer tags, sync `CURRENT_RELEASE_NOTES.md` |
| `update-vendor-cache.ps1` | Copy a built `d3d11.dll` into `vendor/edhm-runtime/` with VERSION/tag metadata |

```powershell
.\scripts\fetch-upstream.ps1
.\scripts\fetch-upstream.ps1 -UpdateBase -PushBase -ShowNew

.\scripts\version.ps1 -Action get
.\scripts\version.ps1 -Action bump-patch
.\scripts\version.ps1 -Action bump-prerelease -PreLabel beta
.\scripts\version.ps1 -Action tag -Channel release
.\scripts\version.ps1 -Action tag -Channel prerelease -PreLabel beta
.\scripts\version.ps1 -Action sync-notes

.\scripts\update-vendor-cache.ps1 -DllPath 'dist\d3d11.dll' -Version 0.1.0 -Tag v0.1.0 -Channel release
```

See `docs/releasing.md` and `vendor/edhm-runtime/README.md`.
