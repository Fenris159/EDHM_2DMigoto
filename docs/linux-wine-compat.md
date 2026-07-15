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
| `dll_initialization_delay` | `250` ms if the key was omitted |
| Chain load | Tries `original_d3d11.dll`, then local `dxvk_d3d11.dll` / `d3d11_dxvk.dll`, then `system32\d3d11.dll` |
| Log | Writes a **host compatibility report** to `d3d11_log.txt` |

Force the profile on any host with:

```ini
[System]
wine_compat = 1
```

Disable all of the above even under Wine:

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

Install DXVK **into the prefix**. Do **not** drop DXVK’s `d3d11.dll` on top of
EDHM’s game-folder `d3d11.dll`. Optional: put DXVK’s DLL beside the game as
`dxvk_d3d11.dll` and/or set `proxy_d3d11=dxvk_d3d11.dll`.

## Files must be next to the game

Place EDHM next to `EliteDangerous64.exe`, not only next to the FDev launcher.

## Did EDHM load?

After a launch (even a crash), open `d3d11_log.txt` in the game folder.

- **No log file** → Wine never loaded EDHM’s `d3d11.dll` (overrides / path).
- **Log exists** → read the `host compatibility report` and chained d3d11 path.

## Related guides

Plain-language handouts for players:

- `.codex/LinuxTroubleshooting/EDHM_Proton_Linux_Setup.txt`
- `.codex/LinuxTroubleshooting/EDHM_Wine_FDev_Launcher_Setup.txt`
