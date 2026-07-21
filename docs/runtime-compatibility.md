# Runtime compatibility boundaries

EDHM_2DMigoto builds a game-local, 3Dmigoto-derived `d3d11.dll` for Elite
Dangerous. It is not a drop-in replacement for every private export or ordinal
provided by the Microsoft system DLL.

## Supported proxy surface

- Install the DLL next to `EliteDangerous64.exe`; never replace the system
  `d3d11.dll` in Windows or a Wine prefix.
- Elite's required `D3D11CreateDevice` import and the maintained 3Dmigoto proxy
  surface are checked after every CI build.
- The maintained names and historical ordinals are listed in
  `scripts/proxy-exports.json` and verified by `scripts/Test-ProxyExports.ps1`.
- Undocumented private Windows exports are intentionally not implemented from
  guessed signatures. Software that imports one of those names can fail before
  this DLL reaches `DllMain` and will not create `d3d11_log.txt`.

The wrapper intercepts the D3D11 device/context interfaces used by EDHM and
wraps `IDXGISwapChain1` through `IDXGISwapChain4` when the host runtime exposes
them. Device2-Device5 and Context2-Context4 queries are denied because their
additional methods are not intercepted; returning the real interface would
silently bypass state tracking.

## Loader lifecycle

The normal EDHM deployment loads the DLL from the game directory. The inherited
injection mode must identify its target and install early D3D11/DXGI hooks from
`DllMain` before the game can call those APIs. This work remains subject to the
Windows loader-lock constraints and is not a general hot-load/hot-unload API.

Process termination deliberately skips nonessential teardown. Explicit dynamic
unload retains hook, TLS, and runtime cleanup, but normal EDHM operation should
leave the proxy loaded for the lifetime of Elite.

## Paths and text

Configuration, shader, include, format, and overlay text conversions use
checked UTF-8 handling, and temporary C locale changes are isolated to the
calling thread. Some inherited shader, hunting, and frame-analysis paths still
use `MAX_PATH` buffers.

Keep the complete generated path below 260 UTF-16 characters, including shader
file names and analysis suffixes. Enabling Windows long-path support does not
expand a fixed application buffer. On Flatpak, a short visible symlink also
does not grant access to its target; see
[Linux / Wine / Proton compatibility](linux-wine-compat.md#flatpak-steam).

An inaccessible shader directory should produce warnings and leave the HUD
unchanged rather than aborting game initialization.

## Crash diagnostics

`[Logging] crash=0` is the normal-play default. With `crash=1` or `crash=2`,
the inherited crash handler writes a minidump from the failing game process.
That is best-effort diagnostics: damaged process state or loader synchronization
can prevent or stall dump creation. The absence of a dump does not mean the DLL
was not loaded and is not proof that no crash occurred.

Use `d3d11_log.txt`, Windows Error Reporting data, and Proton logs alongside any
minidump. Do not enable the crash handler or verbose logging permanently.

## Release validation

Automated gates cover the Release build, malformed DXBC/configuration contracts,
checked UTF-8 behavior, selected COM interface policies, Present device-loss
classification, AddressSanitizer, and the proxy export manifest. They do not
replace live validation on Windows AMD/NVIDIA/Intel systems and Wine/Proton
with DXVK, overlays, display-mode changes, and normal game shutdown.
