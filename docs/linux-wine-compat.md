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

## Flatpak Steam

Flatpak does not block symbolic links, but Steam must be allowed to read the
link target. A `ShaderFixes` link can therefore appear correctly beside
`EliteDangerous64.exe` while still being unusable from inside Steam's sandbox.

Use one of the following installation methods. The physical-directory method
has the fewest permission and path dependencies.

### Method 1: install physical directories (recommended)

Install or copy the EDHM runtime files directly into the Elite product folder
that contains `EliteDangerous64.exe`. In particular, these must be real
directories rather than symbolic links:

```text
elite-dangerous-odyssey-64/
|-- EliteDangerous64.exe
|-- d3d11.dll
|-- d3dx.ini
|-- EDHM-ini/
`-- ShaderFixes/
```

Steam already has access to its game directory, so no additional Flatpak
filesystem permission is required. If EDHM-UI maintains a separate working
copy, configure it to install or synchronize the active files into these real
directories. Copying them only once will not carry later theme changes across
unless EDHM-UI updates the copies.

Confirm that neither directory is a symbolic link:

```bash
test -d "$GAME_DIR/ShaderFixes" && ! test -L "$GAME_DIR/ShaderFixes"
test -d "$GAME_DIR/EDHM-ini" && ! test -L "$GAME_DIR/EDHM-ini"
```

Set `GAME_DIR` to the product folder shown by Steam's **Browse Local Files**
action before running the checks.

### Method 2: allow Steam to read symlink targets

Keep the EDHM-UI symbolic links, but grant the Steam Flatpak read-only access
to their real target directory. First resolve the links from the Elite product
folder:

```bash
GAME_DIR="/absolute/path/to/elite-dangerous-odyssey-64"
readlink -f "$GAME_DIR/ShaderFixes"
readlink -f "$GAME_DIR/EDHM-ini"
```

Find the narrowest parent directory that contains both resolved targets, then
grant Steam access to that real path. Do not pass the symlink path itself:

```bash
TARGET_PARENT="/absolute/path/to/EDHM-UI-managed-files"
flatpak override --user \
  --filesystem="$TARGET_PARENT:ro" \
  com.valvesoftware.Steam
```

Completely exit and restart Steam after changing the override. This method is
also required when the target is below another application's private
`~/.var/app/<app-id>` directory; seeing the link in the game folder does not
make that target visible inside Steam's sandbox.

If only one directory is linked, grant access only to that directory. Avoid
`--filesystem=home` or `--filesystem=host` because EDHM does not need access to
the user's entire home directory or host filesystem.

With either method, the Proton launch option from the previous section is
still required. Filesystem access and `WINEDLLOVERRIDES` solve separate parts
of startup: the former exposes EDHM's shader files, while the latter tells
Wine to load EDHM's local `d3d11.dll`.

## Did EDHM load?

After a launch (even a crash), open `d3d11_log.txt` in the game folder.

- **No log file** -> first confirm `[Logging] enabled=1` and that the game folder
  is writable. Otherwise Wine probably did not load EDHM's `d3d11.dll`.
- **Log exists** -> read the `host compatibility report` and chained D3D11 path.

The report also warns about `PROTON_USE_WINED3D=1` (OpenGL instead of DXVK) and
`PROTON_NO_D3D11=1` (incompatible with Elite and EDHM), and records whether a
Flatpak environment was visible to the game process.

During configuration loading, look for `Shader override directory available`.
An `inaccessible` warning with a Win32 error means the DLL loaded but Wine could
not traverse `ShaderFixes`; for Flatpak, verify the physical-directory install
or the real symlink-target permission described above.

## Keep the setup minimal

Do not add initialization delays, local DXVK copies, proxy DLLs, or extra
graphics overrides unless a specific diagnostic result calls for them.
