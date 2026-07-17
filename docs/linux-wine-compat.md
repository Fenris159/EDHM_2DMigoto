# Linux / Wine / Proton compatibility

EDHM ships as a local `d3d11.dll` next to `EliteDangerous64.exe`. On Linux the
chain must be:

```text
Elite  ->  EDHM d3d11.dll  ->  DXVK (or other) d3d11  ->  Vulkan
```

This tree adds **Wine-aware defaults and logging** so users do less ini trial
and error. It cannot force Wine to load the game-folder DLL; that still needs
an override.

## What the runtime does automatically

When Wine/Proton is detected (`ntdll!wine_get_version`, plus light env checks):

| Behaviour | Effect |
|-----------|--------|
| `wine_compat` auto | On under Wine unless `wine_compat=0` |
| `load_library_redirect` | Forced to `0` (avoids fighting DXVK) |
| `check_foreground_window` | Forced off (Wine focus is flaky) |
| Chain load | Uses the D3D11 implementation supplied by the Proton/Wine prefix |
| Log | Writes a **host compatibility report** to `d3d11_log.txt` |

Force the profile on any host with:

```ini
[System]
wine_compat = 1
```

Disable the automatic System-setting profile even under Wine:

```ini
[System]
wine_compat = 0
```

## What you still must configure outside the DLL

### Proton (Steam)

Launch options:

```text
WINEDLLOVERRIDES="d3d11=n,b;d3dcompiler_47=n,b" %command%
```

### Wine + FDev launcher

Same override on the environment that starts the launcher:

```bash
export WINEPREFIX="$HOME/.wine-elite"
export WINEDLLOVERRIDES="d3d11=n,b;d3dcompiler_47=n,b"
wine /path/to/EDLaunch.exe
```

For Proton, let Steam manage DXVK. For standalone Wine, install DXVK into the
same Wine prefix used to run Elite. Do **not** copy or rename DXVK's `d3d11.dll`
or `dxgi.dll` into the game folder; that folder's `d3d11.dll` is EDHM.

`proxy_d3d11` is a legacy custom-wrapper option and is not part of the standard
EDHM Linux setup.

## Files must be next to the game

Place EDHM next to `EliteDangerous64.exe`, not only next to the FDev launcher.

## Did EDHM load?

After a launch (even a crash), open `d3d11_log.txt` in the game folder.

- **No log file** -> first confirm `[Logging] enabled=1` and that the game folder
  is writable. Otherwise Wine probably did not load EDHM's `d3d11.dll`.
- **Log exists** -> read the `host compatibility report` and chained D3D11 path.

The report also warns about `PROTON_USE_WINED3D=1` (OpenGL instead of DXVK) and
`PROTON_NO_D3D11=1` (incompatible with Elite and EDHM).

## Keep the setup minimal

Do not add initialization delays, local DXVK copies, proxy DLLs, or extra
graphics overrides unless a specific diagnostic result calls for them.
