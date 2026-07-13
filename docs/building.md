# Building EDHM_2DMigoto

## Requirements

- Windows
- Visual Studio 2022 (MSVC toolset; solution targets match XXMI modernization)
- Windows SDK components as required by the DirectX11 project

## Steps

1. Clone this repository.
2. Open `StereovisionHacks.sln` in Visual Studio.
3. Select configuration (`Release` recommended for testing with EDHM).
4. Build the solution or the primary DirectX11 / injector projects.
5. Collect outputs into `dist/` for packaging (optional; `dist/*` is gitignored by default).

## Notes

- Solution and project names still reflect the historical 3Dmigoto “StereovisionHacks” naming even though stereo/3D features were stripped in XXMI.
- First-time builds may need NuGet or SDK repairs depending on your VS install.
- Do not commit intermediate `Debug/`, `Release/`, `.vs/`, or `*.pdb` artifacts; they are covered by `.gitignore`.

## EDHM testing

Document your local deploy path and smoke-test checklist here as you establish a routine (game path, d3dx.ini layout, loader usage).
