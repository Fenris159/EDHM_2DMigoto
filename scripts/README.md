# Scripts

| Script | Description |
|--------|-------------|
| `fetch-upstream.ps1` | Fetch `xxmi` / `3dmigoto`; optionally fast-forward `xxmi-base` |
| `version.ps1` | Read/bump `VERSION`, compute SemVer tags, sync `CURRENT_RELEASE_NOTES.md` |

```powershell
.\scripts\fetch-upstream.ps1
.\scripts\fetch-upstream.ps1 -UpdateBase -PushBase -ShowNew

.\scripts\version.ps1 -Action get
.\scripts\version.ps1 -Action bump-patch
.\scripts\version.ps1 -Action bump-prerelease -PreLabel beta
.\scripts\version.ps1 -Action tag -Channel release
.\scripts\version.ps1 -Action tag -Channel prerelease -PreLabel beta
.\scripts\version.ps1 -Action sync-notes
```

See `docs/releasing.md`.
