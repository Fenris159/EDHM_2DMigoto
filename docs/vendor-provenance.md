# Vendored native dependencies

This repository owns its checked-in dependency snapshots. Changes are never
pushed to their upstream repositories. A refresh must be imported on a local
branch, pinned to an upstream release tag or commit, license-reviewed, and
validated by the complete quality gate before merge.

| Dependency | Repository snapshot | Authoritative upstream | Refresh policy |
|---|---|---|---|
| DirectXTK | Historical DirectX 11 snapshot; the imported commit was not recorded | https://github.com/microsoft/DirectXTK | Import a pinned release or commit and reconcile EDHM project files and local compatibility changes. |
| PCRE2 | `10.30` (`2017-08-14`) header plus x86/x64 libraries | https://github.com/PCRE2Project/pcre2 | Update the header and all four library variants as one pinned release. |
| crc32c-hw | `1.0.5`, identified by the vendored directory name | Origin is not recorded in this repository | Preserve until an authoritative source and matching license can be proven. |
| Nektra NktHookLib | Historical 2010-2013 header plus x86/x64 release/debug libraries; exact version is not recorded | https://www.nektra.com/products/deviare-api-hook-windows/ | Refresh header and libraries together from an official versioned distribution. |

Generated DirectXTK shader headers are outputs, not independently vendored
source. `scripts/quality.ps1 -Mode Shader -Scope All` recompiles every source
entry point in an ignored temporary directory and fails on compiler warnings or
errors without rewriting the checked-in generated headers.

## Required refresh record

Every future dependency update must record:

- upstream release tag or full commit SHA;
- canonical source URL and retrieval date;
- license and notice changes;
- files intentionally omitted from the upstream package;
- EDHM-local patches retained after the import;
- x64, Win32, shader, clang-tidy, and SonarCloud validation results.
