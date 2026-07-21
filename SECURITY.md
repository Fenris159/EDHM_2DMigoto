# Security Policy

## Scope

EDHM_2DMigoto ships a user-mode Direct3D 11 proxy DLL (`d3d11.dll`) that runs
inside the Elite Dangerous game process. In-scope reports include memory
corruption, crashes, hangs, DLL search-order/hijacking issues, insecure
loading of configuration or shader files, and dependency vulnerabilities in
the components listed in `THIRD_PARTY_NOTICES.md`.

## Supported versions

| Version line | Status |
|---|---|
| `0.1.3-alpha.x` (develop pre-releases) | Supported — fixes land on `develop` first |
| Older snapshots | Unsupported — update to the latest pre-release |

## Reporting a vulnerability

- **Preferred:** open a private report via GitHub Security Advisories for
  this repository (*Security* tab → *Advisories* → *Report a vulnerability*).
- Do not open public issues for unpatched security problems.
- Please include: the `d3d11.dll` SHA-256, the `d3d11_log.txt` (redacted as
  described below), Windows/Proton version, GPU/driver, and the exact steps
  or configuration that trigger the issue.

We aim to acknowledge reports within 7 days and to coordinate a fix and
disclosure timeline with the reporter.

## Redaction guidance for logs and dumps

Before attaching `d3d11_log.txt` or `3DM-*.dmp` crash dumps:

- Remove Windows username paths (`C:\Users\...`) if you consider them
  sensitive.
- Crash dumps may contain arbitrary fragments of game and system memory —
  prefer the log plus a textual call stack unless a maintainer asks for the
  dump, and share dumps only through the private advisory channel.
- Never attach account credentials, session tokens, or CD-keys; the mod does
  not handle these, but a raw dump could incidentally contain them.

## Dependency response policy

Third-party components (PCRE2, DirectXTK, crc32c-hw, Deviare-InProcess) are
inventoried with versions and provenance in `THIRD_PARTY_NOTICES.md`.
Upstream security advisories affecting a bundled component are assessed for
actual reachability in this DLL (for example, PCRE2 is only exposed to
user-authored `ShaderRegex` patterns) and either upgraded, patched, or
publicly documented as not-applicable in the next release's notes.

## Hardening notes for users

- The mod only supports Elite Dangerous' actual import contract; it is not a
  general replacement for the system `d3d11.dll` in arbitrary software.
- Keep author/diagnostic settings (`hunting`, `calls`, `debug`, `crash`,
  `track_region_hashes`) at their shipped defaults for normal play.
- A missing `d3d11_log.txt` next to the game executable means the process
  failed before the DLL initialized — check for export/import failures or a
  blocked write location before assuming a crash.
