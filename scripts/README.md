# Scripts

| Script | Description |
|--------|-------------|
| `fetch-upstream.ps1` | Fetch `xxmi` / `3dmigoto`; optionally fast-forward `xxmi-base` |
| `version.ps1` | Read/bump `VERSION`, compute SemVer tags, sync `CURRENT_RELEASE_NOTES.md` |
| `update-vendor-cache.ps1` | Copy a built `d3d11.dll` into `vendor/edhm-runtime/` with VERSION/tag metadata |
| `bootstrap-quality-tools.ps1` | Install pinned LLVM and PSScriptAnalyzer under the ignored `.quality/` directory |
| `quality.ps1` | Format, analyze, and strictly build maintained source |
| `sonar-local.ps1` | Run a real local MSVC Build Wrapper analysis against a dedicated SonarCloud branch |

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

.\scripts\bootstrap-quality-tools.ps1
.\scripts\quality.ps1 -Mode FormatCheck -Scope Changed
.\scripts\quality.ps1 -Mode PowerShell -Scope Changed
.\scripts\quality.ps1 -Mode Build -Scope All
```

See `docs/releasing.md`, `docs/code-quality.md`, and
`vendor/edhm-runtime/README.md`.
