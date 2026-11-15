# Contributing to EDHM_2DMigoto

Start with [AGENTS.md](AGENTS.md) for the repository contract and task-specific
reading. This guide supplies the fork, implementation and review workflow for
human contributors and coding agents.

## Workflow

Follow the branch roles and authorization rules in
[AGENTS.md](AGENTS.md#project-and-workflow). The example below shows how to
contribute to the project's `develop` branch from a fork.

For a new fork, after cloning it and entering the checkout:

```bash
git remote add upstream https://github.com/Fenris159/EDHM_2DMigoto.git
git fetch upstream develop
git switch -c resource-view-fix upstream/develop
```

Use a name that describes your change. Check existing remotes before adding one;
`origin` normally points to your fork in this example. Push the feature branch
to your fork and select `Fenris159/EDHM_2DMigoto:develop` as the PR base. An
independent fork can choose its own integration branch deliberately.

In an existing checkout, inspect `git status`, `git branch --show-current`,
`git remote -v` and `git worktree list` first. Preserve unrelated changes. A
separate worktree is useful for an isolated patch, benchmark variant or mutation
run. Compare the actual base and head SHAs; equal branch names on different
remotes do not establish equivalent source.

Keep a change focused enough to review independently. Preserve the minimal
EDHM build graph when porting upstream work; broad legacy cleanup and unrelated
dependency refreshes belong in separate changes. Explain upstream adaptations
and compatibility tradeoffs in the PR.

## Source map

| Area | Start here |
| --- | --- |
| Product and linked dependencies | `EDHM_2DMigoto.sln`, `DirectX11/DirectX11.vcxproj`, [layout](docs/layout.md) |
| Command parser, expressions, copies, pools, readback and views | `DirectX11/CommandList.cpp`, `CommandList.h`, [runtime contracts](DirectX11/AGENTS.md) |
| Configuration, reference finalization, reload and persistent settings | `DirectX11/IniHandler.cpp`, `IniHandler.h`, `PersistentSettingsState.h` |
| Hooked D3D calls and context state | `DirectX11/HackerContext.*`, `HackerDXGI.cpp`, `globals.h` |
| Resource tracking and CPU hash/cache data | `DirectX11/ResourceHash.*` |
| Profiling and on-screen reporting | `DirectX11/profiling.*`, `Overlay.*` |
| Shader and draw-context hunting | `DirectX11/AdvancedHunting*`, `Hunting.*`, `AdvancedHuntingTests/` |
| Production-path regressions and D3D boundary fixtures | `RuntimeTests/`, `DirectX11/RuntimeTests.vcxproj` |
| Packages and default configuration | `Dependencies/`, `vendor/edhm-runtime/`, [releasing](docs/releasing.md) |

The linked project files define the executable build graph. Full XXMI components
outside this graph remain comparison material on `xxmi-base`. Refresh imported
snapshots according to [the quality policy](docs/code-quality.md), preserving
licenses and upstream provenance.

## Build and validate

Production builds use Windows, Visual Studio 2022/MSVC v143 and the Windows SDK.
Run the following from a developer PowerShell at the repository root:

```powershell
.\scripts\bootstrap-quality-tools.ps1
.\scripts\quality.ps1 -Mode FormatCheck -Scope Changed -BaseRef origin/develop
.\scripts\quality.ps1 -Mode PowerShell -Scope Changed -BaseRef origin/develop
.\scripts\quality.ps1 -Mode Build -Scope All
```

Use the remote containing the intended base in `-BaseRef`; for the fork example
above it is `upstream/develop`. Use `-Mode Format` to apply formatting before the
format check. Tool pins live in the bootstrap and quality scripts. The build
mode performs strict Release builds for x64 and Win32; C++17 and compatibility
definitions also apply with strict warnings disabled.

Validate ordinary Release and Zip Release when changing language settings,
project files or packaging. Strict-only success can hide a default-build failure.
The [CI workflow](.github/workflows/ci.yml) defines the current commands and its
manual `zip_release` input adds strict Zip Release validation for both platforms.
Shader changes also require `quality.ps1 -Mode Shader -Scope All`. Follow
[the quality workflow](docs/code-quality.md) for a real compilation database,
clang-tidy and Sonar analysis.

On a host without MSVC or D3D, run the applicable portable checks and state which
Windows checks remain unverified. CI results for the exact source revision can
supply that evidence. Fork PRs may lack access to maintainer secrets or external
analysis services; report a skipped check separately from a passing one.

For documentation-only changes, verify paths, links, command examples and
whitespace. Run broader checks when the changed documentation or configuration
affects agent loading or CI behavior; native runtime rebuilds are unnecessary for
prose alone.

## Testing

When asked to write tests, list the observable behaviors and boundary/error cases
in plain language and obtain confirmation before writing them. Cases already
approved in the conversation remain approved. Check existing coverage first and
prefer extending one focused case over adding near-duplicates.

- Derive expected values from the requirement, using hand-computed literals.
  Test public behavior and outcomes rather than private methods or call order.
- Cover the happy path, meaningful boundaries and relevant failures. Mock only
  external boundaries such as a device, filesystem, process, network or clock.
- Keep cases independent and hermetic; use explicit frame numbers and controlled
  inputs instead of wall-clock timing, unseeded randomness or shared test state.
- Prove sensitivity by deliberately breaking the behavior and observing the
  focused case fail. Restore production source and rebuild the executable before
  accepting the result. A mutant that fails to compile is not a detected defect.
- Fix production code when an existing valid test fails. Preserve its requirement
  and strong assertions; coverage percentage is not the acceptance criterion.

For parser, settings, pool or D3D changes, use the production-path native harness:

```powershell
msbuild DirectX11/RuntimeTests.vcxproj -target:Build `
  -property:Configuration=Release -property:Platform=x64 `
  "-property:SolutionDir=$pwd/" -property:EdhmStrictBuild=true `
  -nodeReuse:false -verbosity:minimal -restore:false
& .quality/runtime-tests/x64/Release/RuntimeTests.exe region-cb-small
```

Replace the case with the behavior you changed. Use `Platform=Win32` and the
corresponding executable directory for 32-bit validation. New cases need an
entry in the native case registry and the selection in
[the runtime workflow](.github/workflows/runtime-tests.yml). See
[RuntimeTests/README.md](RuntimeTests/README.md) for D3D/WARP requirements,
mutation verification and benchmark modes. Hunting-specific state checks live
in `AdvancedHuntingTests/`; its MSBuild project runs them after building.

Run `RuntimeTests/check_mutations.py` only in a clean disposable checkout. It
temporarily changes production sources, restores their exact bytes, then
rebuilds and checks production. If restoration fails, remove or quarantine the
executable and retain the original error. The runner's cleanup cases are:

```bash
python -m unittest discover -s RuntimeTests -p test_mutation_cleanup.py -v
```

Native D3D tests use WARP and request the debug layer. Report a missing layer as a
limitation. They do not establish installed Elite Dangerous, hardware-GPU or
Proton/DXVK compatibility. Game-affecting changes need the separately authorized
smoke checks: HUD/theme rendering, location loading, docking, reload, persistent
settings and shutdown. Keep the original configuration and DLL recoverable.

## Measurements

Use Elite Dangerous + EDHM as the target workload. Separate a correctness repair
from an optimization claim. For an improvement claim, isolate the change in an
A/B comparison and record source SHAs, executable hashes, configuration, host,
diagnostic modes, raw samples and the method of summarizing them.

Keep logging, hunting and profiling modes consistent. Measure frame-time
outcomes with diagnostics disabled and use separate diagnostic captures to
locate costs. Report startup time, synthetic CPU mechanisms, GPU time and game
frame time as distinct outcomes. A reduction in Create*View calls alone does not
establish an FPS gain.

The [implementation report](docs/next-release-implementation.md) explains past
accept/reject decisions; `docs/benchmarks/` holds pinned historical evidence.
Keep old receipts immutable and add a new receipt for a new source or binary.
Recheck relevance against current source rather than assuming the audit's
candidate, deferred commits or release status is still current.

## Upstream, versions and releases

Before a port, read [the upstream workflow](docs/upstream-sync.md) and
[the intake decisions](docs/upstream-next-release.md). Record the actual upstream
repository and SHA, prerequisites, local adaptations and evidence of EDHM value.
Port selectively into the slim tree. Advancing `xxmi-base` and accepting a change
into the runtime are separate operations.

For version work, [the release guide](docs/releasing.md) defines the channels.
Keep the `VERSION` package core synchronized with `version.h`'s numeric DLL
version. `scripts/version.ps1` updates `VERSION` and can synchronize release
notes; it does not update `version.h`. Maintain the matching changelog section
and regenerate `CURRENT_RELEASE_NOTES.md` with `-Action sync-notes`.

Merging, versioning, tagging, publishing and updating the binary vendor cache
are distinct steps. Carry out the requested steps using existing authorization;
a prepared PR or version bump alone is not a release publication or a game
installation. Follow [the release workflow](.github/workflows/release.yml) for
authorized release operations.

## Review guidance

Write the PR for a reviewer without the conversation: identify the trigger and
resulting behavior, scope, upstream provenance where applicable, validation with
the tested SHA, and material compatibility or performance limits. Link relevant
command documentation and evidence. Explain any remaining failures or skipped
checks explicitly.

Review actual control flow, COM ownership, failure paths and reload lifetimes
against [the runtime contracts](DirectX11/AGENTS.md). Support findings with a
specific input, affected code and observable consequence. Distinguish a statically
established defect from an executed reproduction. Check formatter claims with
the repository's pinned formatter, and distinguish inherited code from changes
introduced by the patch.

Read the current review summary and unresolved threads, not only a check's
success status: an automated reviewer can complete successfully while reporting
defects. Treat historical receipts as evidence for their pinned revisions, not
as tests of the current PR head.

## Agent entry points

The shared guidance lives in `AGENTS.md` and its task-specific links. The small
adapters contain no separate policy:

| Entry point | Purpose |
| --- | --- |
| `AGENTS.md` | Root instructions, including [Kilo Code's supported discovery](https://kilo.ai/docs/customize/agents-md) |
| `DirectX11/AGENTS.md` | Runtime contracts scoped to that directory and explicitly linked for shared helpers |
| `CLAUDE.md` | Imports `AGENTS.md` using [Claude Code's file-import syntax](https://code.claude.com/docs/en/memory#import-additional-files) |
| `.github/copilot-instructions.md` | Points to shared guidance for [Copilot repository instructions](https://docs.github.com/en/copilot/concepts/prompting-and-context/custom-instructions) |
| `REVIEWS.md` | Review entry point for [Kilo Code Reviews](https://blog.kilo.ai/p/code-reviews-md) when its `REVIEWS.md` option is enabled |

Client settings can disable instruction loading. For a fork, verify that the
client loaded the root file before relying on it. Kilo's hosted review toggle is
an integration setting; adding `REVIEWS.md` does not enable that setting by itself.
Keep setup instructions portable and update the owning document when a
convention changes.
