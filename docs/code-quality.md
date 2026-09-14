# Code quality workflow

## Tool versions

- LLVM `23.1.1` provides `clang-format` and `clang-tidy`.
- PSScriptAnalyzer `1.25.0` analyzes repository PowerShell.
- SonarScanner CLI `8.1.0.6389` performs optional local SonarCloud uploads.
- Visual Studio 2022 with the Desktop development with C++ workload provides
  MSBuild, MSVC v143, and the Windows SDK.

Run the repository-local bootstrap once:

```powershell
.\scripts\bootstrap-quality-tools.ps1
```

The bootstrap stores tools under the ignored `.quality` directory. It verifies
the pinned LLVM MSI SHA-256 before extracting it and does not add tools to the
system `PATH`.

## Routine local checks

Check files changed from `origin/main`:

```powershell
.\scripts\quality.ps1 -Mode FormatCheck -Scope Changed
.\scripts\quality.ps1 -Mode PowerShell -Scope Changed
```

Format changed native files:

```powershell
.\scripts\quality.ps1 -Mode Format -Scope Changed
```

Run clean, strict Release builds for both architectures:

```powershell
.\scripts\quality.ps1 -Mode Build -Scope All
```

## SonarQube for Visual Studio

Install SonarQube for Visual Studio and bind the solution to:

- Service: `https://sonarcloud.io`
- Organization: `fenris159`
- Project: `Fenris159_EDHM_2DMigoto`

Connected mode applies the SonarCloud quality profile and exclusions for fast
editor feedback. It does not replace the Build Wrapper scan because some
CFamily findings require the complete compilation database and server analysis.

## Full local SonarCloud analysis

Set a user token only in the current process and scan a dedicated branch:

```powershell
$env:SONAR_TOKEN = '<user token>'
.\scripts\sonar-local.ps1 -Branch 'local/fenris-quality' -RunClangTidy
Remove-Item Env:\SONAR_TOKEN
```

The script downloads the current project Build Wrapper and the pinned scanner,
performs a clean MSVC `Release|x64` build, optionally runs clang-tidy against the
captured compilation database, and uploads a real SonarCloud branch analysis.

## Scope policy

Formatting applies to maintained C and C++ source, including inherited
3Dmigoto/XXMI and DirectXTK code. Generated shader includes and source snapshots
maintained by separate upstream projects (`pcre2`, `Nektra`, and `crc32c-hw`)
are not reformatted locally. They remain subject to Sonar correctness analysis
and are refreshed from authoritative upstream releases instead of being
restyled by this repository.

No analyzer warning may be hidden solely to make a check pass. False positives
must be reviewed individually and documented in SonarCloud.

The only repository-level Sonar exception is `cppsecurity:S2083` in
`DirectX11/HackerDevice.cpp` and `DirectX11/Hunting.cpp`. Sonar reports shader
byte buffers passed to `fwrite` as filesystem paths. Those values are file
contents rather than path components, so path traversal is impossible at the
reported sinks.

`cppsecurity:S5145` is excluded for C++ implementation files because this
project writes only local developer diagnostics, not security or audit logs.
The logging macros retain fixed format strings, so attacker-controlled format
execution is still prevented; embedded line breaks cannot affect authorization,
monitoring, or another trust boundary.

`cpp:S936` is excluded only in the four hook-registration implementation files.
The reported function designators are arguments to token-pasting hook macros;
adding `&` would prevent those macros from deriving the paired trampoline and
original-function symbols. The macro expansion already uses them as function
pointers.
