# Scripts

| Script | Description |
|--------|-------------|
| `fetch-upstream.ps1` | Fetch `xxmi` / `3dmigoto`; optionally fast-forward `xxmi-base` and list new commits |

```powershell
.\scripts\fetch-upstream.ps1
.\scripts\fetch-upstream.ps1 -ShowNew
.\scripts\fetch-upstream.ps1 -UpdateBase -ShowNew
.\scripts\fetch-upstream.ps1 -UpdateBase -PushBase -ShowNew
```
