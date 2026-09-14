# Code quality modernization plan

## Objective

Bring every maintained source file in this repository to a uniform, locally
verifiable quality baseline without publishing changes to the XXMI, 3Dmigoto,
or other upstream repositories.

Generated artifacts remain excluded from source analysis. Vendored libraries
may be refreshed from their authoritative upstream releases, but all resulting
changes remain in this repository.

## Definition of clean

- SonarCloud reports zero open issues and zero accepted issues on `main`.
- Only individually reviewed, documented false positives remain.
- Release builds succeed for both `x64` and `Win32`.
- New C and C++ changes pass the repository's pinned `clang-format` policy.
- C and C++ changes pass the selected `clang-tidy` and MSVC analysis policies.
- PowerShell changes pass PSScriptAnalyzer.
- HLSL and FX sources have an explicit compile-validation path.
- CI and local scripts use the same commands and configuration.
- Formatting-only changes remain separate from behavioral fixes.

## Starting point

Snapshot taken from SonarCloud on 2026-09-14:

| State | Count |
|---|---:|
| Open or confirmed | 0 |
| Accepted | 9,243 |
| False positive | 4 |
| Accepted vulnerabilities | 542 |
| Accepted bugs | 94 |
| Accepted code smells | 8,607 |

Largest accepted directories:

| Directory | Count |
|---|---:|
| `DirectX11` | 4,609 |
| `HLSLDecompiler` | 1,460 |
| `DirectXTK/Src` | 1,212 |
| Repository root | 476 |
| `DirectXTK/Inc` | 369 |
| `D3D_Shaders` | 343 |
| `BinaryDecompiler` and headers | 568 |

## Work plan

### Phase 0: establish a safe baseline

- [x] Create `codex/code-quality-modernization` from current `origin/main`.
- [x] Preserve the user's existing `develop` checkout unchanged.
- [x] Capture current SonarCloud accepted/open/false-positive counts.
- [x] Confirm the Windows MSVC Build Wrapper analysis is present on `main`.

### Phase 1: local and CI quality foundation

- [x] Add `.editorconfig` for text and native-source fundamentals.
- [x] Add `.clang-format` matching the repository's established native style.
- [x] Add `.clang-tidy` with correctness-first C++14-compatible checks.
- [x] Add PSScriptAnalyzer configuration.
- [x] Add local format, analysis, build, and Sonar wrapper scripts.
- [x] Add shared MSBuild quality properties without weakening vendor builds.
- [x] Add CI format/static-analysis jobs and both-architecture builds.
- [x] Remove blanket Sonar rule suppressions; retain generated-only exclusions.
- [x] Document prerequisites and Visual Studio connected-mode setup.

### Phase 2: correctness and security remediation

- [x] Resolve all Blocker findings reported by a fresh branch analysis.
- [x] Resolve all vulnerabilities reported by a fresh branch analysis.
- [x] Audit logging-injection findings by sink and preserve fixed formats.
- [x] Resolve all bugs reported by a fresh branch analysis.
- [x] Keep reviewed compatibility exceptions path-scoped with written rationale.

### Phase 3: mechanical modernization and formatting

- [x] Apply safe, rule-specific transformations in isolated commits.
- [x] Replace unsafe string APIs and redundant constructs where semantics permit.
- [x] Normalize declarations, initialization, constness, and ownership boundaries.
- [x] Format maintained C and C++ source by component.
- [x] Keep generated outputs and binary/vendor snapshots untouched.

### Phase 4: structural maintainability remediation

- [x] Review complexity findings and avoid behavior-risking parser/hook rewrites.
- [x] Replace local unsafe ownership with RAII and make ABI transfers explicit.
- [x] Resolve unsafe copying and polymorphic destruction findings.
- [x] Delegate purely structural thresholds to the documented project profile.

### Phase 5: vendored library refresh and policy

- [x] Record all recoverable DirectXTK, PCRE2, crc32c, and Nektra provenance.
- [x] Require future refreshes to use pinned authoritative releases or commits.
- [x] Document missing historical revisions instead of guessing them.
- [x] Analyze the current snapshots under the same correctness policy.

### Phase 6: final verification and rollout

- [x] Run format verification over every maintained source file.
- [x] Run PSScriptAnalyzer over every repository PowerShell script.
- [x] Run clean Release builds for `x64` and `Win32`.
- [x] Run MSVC Code Analysis and the selected clang-tidy checks.
- [x] Compile every DirectXTK HLSL/FX entry point with `fxc /WX`.
- [x] Run real SonarCloud branch analyses through Build Wrapper.
- [ ] Confirm the final SonarCloud quality gate passes with zero open issues.
- [ ] Reconcile legacy accepted findings on `main` after merge.
- [ ] Update the progress log with final cloud and merge results.

## Operating rules

- No changes are pushed or proposed to external upstream repositories.
- Do not suppress a finding merely to reach zero.
- Treat generated files separately from maintained source.
- Treat a vendored dependency update as a source/provenance change, not a style edit.
- Keep automatic formatting separate from semantic remediation.
- Preserve C++14 and the current MSVC/Windows runtime contract unless a deliberate
  compatibility change is documented and validated.
- Keep `xxmi-base` usable as the upstream mirror; modernization lives on the
  project branch and is reconciled locally when upstream updates are imported.

## Progress log

- 2026-09-14: Baseline captured from `origin/main` at `e5b346af`; implementation
  branch and isolated worktree created.
- 2026-09-14: Added the pinned local quality toolchain, strict dual-architecture
  build policy, changed-file format/tidy gates, and local SonarCloud wrapper.
- 2026-09-14: Reduced a valid branch scan from 2,711 findings (7 bugs, 348
  vulnerabilities, 2,356 smells) to 0 bugs and 0 vulnerabilities before final
  profile reconciliation; completed the all-source local gate.
- 2026-09-14: Added non-destructive `/WX` shader compilation and documented the
  recoverable provenance and refresh policy for every native dependency snapshot.
