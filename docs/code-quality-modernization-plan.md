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

- [ ] Resolve all accepted Blocker findings.
- [ ] Resolve all accepted vulnerabilities, starting with non-logging rules.
- [ ] Audit logging-injection findings by sink and sanitize untrusted data.
- [ ] Resolve all accepted bugs and add focused regression checks where viable.
- [ ] Reclassify only demonstrated analyzer false positives with rationale.

### Phase 3: mechanical modernization and formatting

- [ ] Apply safe, rule-specific transformations in isolated commits.
- [ ] Replace legacy null literals, unsafe string APIs, and redundant constructs.
- [ ] Normalize declarations, initialization lists, constness, and aliases.
- [ ] Format maintained C and C++ source by component.
- [ ] Keep generated outputs and binary/vendor snapshots untouched.

### Phase 4: structural maintainability remediation

- [ ] Reduce excessive nesting and cognitive complexity without changing behavior.
- [ ] Replace unsafe ownership patterns with scoped resource management.
- [ ] Resolve copying, inheritance, enum, and interface design findings.
- [ ] Simplify large functions/classes only behind stable existing interfaces.

### Phase 5: vendored library refresh and policy

- [ ] Identify exact DirectXTK, PCRE2, crc32c, and Nektra provenance.
- [ ] Refresh source vendoring only from authoritative upstream releases.
- [ ] Record versions, source URLs, licenses, and local patch differences.
- [ ] Analyze refreshed source under the same correctness policy.

### Phase 6: final verification and rollout

- [ ] Run format verification over every maintained source file.
- [ ] Run PSScriptAnalyzer over every repository PowerShell script.
- [ ] Run clean Release builds for `x64` and `Win32`.
- [ ] Run MSVC Code Analysis and the selected clang-tidy checks.
- [ ] Run a real SonarCloud branch analysis through Build Wrapper.
- [ ] Reopen accepted findings in controlled batches and verify zero remaining.
- [ ] Confirm the SonarCloud quality gate passes with zero accepted/open issues.
- [ ] Update documentation with the final commands and measured results.

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
