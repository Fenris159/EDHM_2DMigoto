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

Compile every DirectXTK shader entry point with warnings as errors in an
ignored temporary tree:

```powershell
.\scripts\quality.ps1 -Mode Shader -Scope All
```

Run the complete local gate with a captured compilation database:

```powershell
.\scripts\quality.ps1 -Mode All -Scope All -CompileDatabase .quality\compile_commands\compile_commands.json
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

## Scope and quality-profile policy

Formatting applies to maintained C and C++ source, including inherited
3Dmigoto/XXMI and DirectXTK code. Generated shader includes and source snapshots
maintained by separate upstream projects (`pcre2`, `Nektra`, and `crc32c-hw`)
are not reformatted locally. They are refreshed from authoritative upstream
releases instead of being restyled in this repository.

The quality contract is correctness-first. Strict MSVC builds, the pinned
clang-tidy checks, and Sonar bug, vulnerability, bounds, reachability,
fallthrough, const-safety, polymorphic-destruction, and rule-of-five rules remain
blocking. Sonar rules that only prescribe a competing C++ style or demand broad
legacy redesign are disabled in `sonar-project.properties`; formatting and safe
mechanical modernization are owned by the local tools instead. This avoids
unsafe rewrites of hook macros, COM wrappers, binary parsers, and imported
DirectX interfaces while keeping real defects visible.

Copy-paste detection excludes `HLSLDecompiler/DecompileHLSL.cpp`, whose emitter
necessarily repeats target-language syntax templates, and `DirectXTK/**`, whose
snapshot intentionally preserves upstream repetition. These files remain fully
included in compilation and Sonar bug, vulnerability, and issue analysis.

Path-scoped exceptions are limited to reviewed compatibility boundaries:

- `cppsecurity:S2083`: shader byte buffers are file contents, not path inputs.
- `cppsecurity:S5145`: C++ logs are local developer diagnostics, not audit logs;
  all format strings remain fixed.
- `cpp:S936`: hook function designators feed token-pasting registration macros.
- `cpp:S3471`: COM method declarations use Windows calling-convention macros.
- `cpp:S3624`, `cpp:S4962`, `cpp:S7119`: the DirectXTK code already implements
  the required ownership semantics or follows Windows SDK constant conventions.
- `cpp:S5025`: raw allocations at listed files are C/COM ownership transfers,
  returned ABI objects, or buffers whose lifetime is released at the matching
  boundary; ordinary local ownership uses RAII.
- `cpp:S5276`: listed conversions are explicit rendering and timing projections.
- `cpp:S1181` and `cpp:S2738`: listed catch-all handlers are process/driver
  boundaries that must prevent exceptions from crossing a C or COM ABI.
- `cpp:S5813`: listed `strlen`/`wcslen` calls consume fixed arrays, literals, or
  buffers whose producing Windows/parser API guarantees null termination.
- `cpp:S923`: listed variadic functions implement established C-compatible
  callback and formatted-logging boundaries.
- `cpp:S108`, `cpp:S1186`, and `cpp:S1144`: listed no-op and conditionally used
  functions implement callback, template, platform, or interface contracts.

A new exception requires the same narrow resource scope and a rationale here.
Do not broaden an exception when the underlying code can be corrected safely.
