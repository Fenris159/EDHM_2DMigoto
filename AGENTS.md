# Agent guidance

This file applies to the repository. Read it before planning changes; read any
applicable directory-level `AGENTS.md` before editing there.

## Project and workflow

EDHM_2DMigoto is an Elite Dangerous + EDHM compatibility build of the
3Dmigoto-derived XXMI/2Dmigoto runtime. The product is `DirectX11`'s `d3d11.dll`.
Preserve upstream authorship, licensing, product attribution and the slim build
graph. `d3dcompiler_47.dll` is a Microsoft redistributable, not a project output.

- Use `develop` as the integration base and normal PR target. `main` receives
  release promotions from `develop`.
- Keep `xxmi-base` an exact upstream reference; select and adapt changes into
  `develop`. The `badges` branch holds generated status data.
- Use descriptive branch names without a `codex/` prefix unless requested.
- Start by checking the branch, worktree status, remotes and relevant commit
  SHAs. A fork's `origin` may be the contributor's repository. Preserve unrelated
  local changes; use a separate worktree when isolation is needed.
- Follow the task's authorization for commits, pushes and merges. Preparing a
  change does not itself authorize publishing a release or installing a DLL into
  a game. Reuse authorization already given in the conversation.

## Read for the task

| Task | Required context before changing it |
| --- | --- |
| Fork setup or PR preparation | [Contributor workflow](CONTRIBUTING.md#workflow) and [review expectations](CONTRIBUTING.md#review-guidance) |
| Locate the implementation | [Source map](CONTRIBUTING.md#source-map) and [build graph](docs/layout.md) |
| Runtime behavior, D3D resources, parser or shared runtime helpers | [DirectX11/AGENTS.md](DirectX11/AGENTS.md), including when the edited helper lives outside `DirectX11/` |
| INI commands or configuration compatibility | [Command reference](docs/ini-command-reference.md) and the corresponding production-path cases in `RuntimeTests/` |
| Native build, formatting or analysis | [Build guide](docs/building.md), [quality workflow](docs/code-quality.md), `Directory.Build.props`, `Directory.Build.targets`, and the relevant `.github/workflows/` file |
| Test design or regression repair | [Testing](CONTRIBUTING.md#testing) and [native runtime checks](RuntimeTests/README.md) |
| Upstream intake or architectural changes | [Upstream workflow](docs/upstream-sync.md), [layout](docs/layout.md), [intake decisions](docs/upstream-next-release.md), and [implementation report](docs/next-release-implementation.md) |
| Performance work | [Measurements](CONTRIBUTING.md#measurements) and the relevant report and raw evidence under `docs/benchmarks/` |
| Version, packaging or release | [Release workflow](docs/releasing.md), `VERSION`, `version.h`, `CHANGELOG.md`, `CURRENT_RELEASE_NOTES.md`, and `scripts/version.ps1` |
| Code review | [Review guidance](CONTRIBUTING.md#review-guidance); follow the patch's actual base and head |

## Quality and completion

- Use the repository's pinned tools through `scripts/bootstrap-quality-tools.ps1`
  and `scripts/quality.ps1`. Read the current pins rather than substituting a
  system formatter. C++17 is required in ordinary and strict builds.
- Keep correctness checks active. Use the exception policy in
  `docs/code-quality.md` for justified compatibility boundaries; fix defects
  instead of weakening a test, analyzer rule or warning policy.
- Treat dated audits, benchmark receipts and old branch names as historical
  checkpoints. Verify current refs and executable configuration in source and
  workflow files; report discrepancies instead of copying stale assumptions.
- Finish with the behavior changed, files affected, checks actually run and
  remaining limitations. Distinguish a build, a WARP test, a synthetic CPU
  measurement and an installed-game result.

## Maintaining these instructions

Keep shared instructions here, contributor procedures in `CONTRIBUTING.md`, and
runtime-specific contracts in `DirectX11/AGENTS.md`. `CLAUDE.md`,
`.github/copilot-instructions.md` and `REVIEWS.md` are entry-point adapters;
update their targets rather than copying policies into each one. Keep guidance
portable: machine paths, credentials and a developer's installed game belong in
local configuration, not repository instructions.
