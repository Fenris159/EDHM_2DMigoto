# Building EDHM_2DMigoto

## Requirements

- Windows
- Visual Studio 2022 (toolset **v143**)
- Windows 10/11 SDK (for headers and `d3dcompiler_47.dll` redist)

## Build `d3d11.dll`

1. Open **`EDHM_2DMigoto.sln`** (not the old XXMI `StereovisionHacks.sln` — that lives only on `xxmi-base`).
2. Select configuration:
   - **Release | x64** for normal testing
   - **Zip Release | x64** for packaging-style output (if configured)
3. Build project **DirectX11** (or the whole solution).
4. Find `d3d11.dll` under the platform/config output directory (e.g. `x64\Release\` or `x64\Zip Release\`).

Dependent static libraries (`BinaryDecompiler`, `DirectXTK_Desktop_2017`) build automatically via project references.

## `d3dcompiler_47.dll`

This DLL is **not** produced by our C++ projects. DirectX11 post-build steps copy it from:

```text
$(WindowsSdkDir)redist\d3d\x64\d3dcompiler_47.dll
```

A copy is also committed under `Dependencies\d3dcompiler_47.dll` for EDHM packaging when the SDK path is inconvenient.

## Packaging for EDHM smoke tests

Typical Odyssey **product root** (next to `EliteDangerous64.exe`, not only the Steam library root):

```text
d3d11.dll              ← build this project (do not replace dxgi.dll if ReShade owns it)
d3dcompiler_47.dll     ← keep EDHM’s MS redistributable (or Dependencies/)
d3dx.ini + EDHM-ini/ + ShaderFixes/   ← from EDHM
ShaderCache/           ← created at runtime when hunting/export is active
```

Regular Windows installs may use EDHM-UI junctions. Flatpak Steam users should
prefer physical `EDHM-ini/` and `ShaderFixes/` directories in the product root;
symlink targets outside the game directory require an explicit sandbox
permission. See [Linux / Wine / Proton compatibility](linux-wine-compat.md#flatpak-steam).

Drop your built `d3d11.dll` over EDHM’s without modifying EDHM inis/shaders.  
Live installs may run stock **3Dmigoto 1.4.5 or 1.4.9** (check file properties).  
Canonical docs: [Wiki](https://github.com/Fenris159/EDHM_2DMigoto/wiki). Compatibility notes: `patches/0001-edhm-xxmi-compat.md`.

Copy finished artifacts into `dist/` locally if useful (`dist/*` is gitignored except README).

## What not to expect

- No XXMI Launcher injector build on `develop`
- No `D3DCompiler_46` proxy project on `develop`
- Full upstream tree remains on `xxmi-base` for reference and cherry-picks
