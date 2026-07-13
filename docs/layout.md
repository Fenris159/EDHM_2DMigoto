# Repository layout

## Three layers

| Layer | Branch | Contents |
|-------|--------|----------|
| Full upstream | `xxmi-base` | Entire XXMI-Libs-Package tree (mirror of SpectrumQT) |
| Working slim | `develop` | Only what is needed to build EDHM’s `d3d11.dll` (+ package `d3dcompiler_47.dll`) |
| Release | `main` | Scaffold + accepted `develop` merges |

## What EDHM needs

| File | How we get it |
|------|----------------|
| `d3d11.dll` | **Build** `DirectX11` project |
| `d3dcompiler_47.dll` | **Microsoft redistributable** — post-build xcopy from Windows SDK; also vendored under `Dependencies/d3dcompiler_47.dll` for packaging. This is **not** the old 3Dmigoto `D3DCompiler_46` proxy project. |

## `develop` build graph

```text
                    ┌─────────────────────┐
                    │  DirectXTK_Desktop  │  static lib
                    │       _2017         │
                    └──────────┬──────────┘
                               │
┌──────────────────┐           │
│ BinaryDecompiler │ static    │
└────────┬─────────┘           │
         │                     │
         ▼                     ▼
┌──────────────────────────────────────────┐
│              DirectX11                   │  ──►  d3d11.dll
│  + D3D_Shaders/*.cpp (compiled in)       │
│  + HLSLDecompiler/DecompileHLSL.cpp      │
│  + crc32c, util, ini_parser, …           │
│  + Nektra + pcre2 prebuilt libs          │
└──────────────────────────────────────────┘
```

## Kept on `develop` (from XXMI)

- `DirectX11/`
- `BinaryDecompiler/`
- `D3D_Shaders/` sources used by DirectX11 (`Assembler.cpp`, `SignatureParser.cpp`, headers)
- `HLSLDecompiler/DecompileHLSL.*` (not the cmd-line tool)
- `DirectXTK/` slim: `Inc/`, `Src/`, `DirectXTK_Desktop_2017.*`, license
- `Nektra/`, `pcre2/` prebuilts
- `crc32c-hw-1.0.5/`
- `Dependencies/`
- Shared root headers/sources used by DirectX11
- Overlay fonts (`*.spritefont`)

## Dropped from `develop` (still on `xxmi-base`)

- Injector / InjectorLib (XXMI loader path; EDHM uses its own deployment)
- `DirectXGI/`
- `D3DCompiler_*` proxy projects (39–46) and `D3DCompiler/` wrapper sources
- `7zip/`, `TestShaders/`
- Full `pcre2-10.30` sources (we link prebuilt `pcre2/`)
- Unused DirectXTK platforms (Phone, Xbox, Win8.1, Audio, MakeSpriteFont, …)
- Full `StereovisionHacks.sln` (replaced by `EDHM_2DMigoto.sln`)

When a cherry-pick from `xxmi-base` touches a dropped path, either ignore it or re-evaluate whether `develop` needs that component for EDHM.
