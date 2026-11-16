# Edo-next preparation: pre-production implementation

Session: 16 November 2026, 18:59–21:59 UTC. Branch:
`pre-production-edo-readiness`, based on `develop` at
`4ae925945879a897309992f58db6cc752f22289d`. Remote `develop` and `main` were
rechecked during the session and matched that base. The older original checkout
and release branches are untouched. No merge, release, version bump or game
installation is part of this session.

The [main plan](edo-next-readiness-2026-11-16.md) remains the work order. These
changes are preliminary; native validation gates still apply.
The branch is local: no push or PR was created during this session.

## Checkpoints

| Plan item | Preliminary result | Remaining gate |
| --- | --- | --- |
| Rank 1: upload safety/retry | Checked Map and retained requests, including publication after transition entries retire | Actual runtime transition/command cases on Windows |
| Rank 2: passive inventory | Implemented standard-library tool with include graph, exact digests, matching fields, row references and explicit unknown provenance | Original game containers/build provenance and further semantic-cost inventory |
| 2A: publication ownership | Shared context-owned pending policy and concrete D3D adapter; original CPU vector, upload points and transition request frequency retained | Strict MSVC x64/Win32 builds and product/WARP regressions |
| 2C: empty-list path | Experimental guarded path and public-entry benchmark drafted; pending retries still run | Matched native A/B measurements and `entry-path` regression/mutation checks |
| 2B / 2D / 2E | Retained as gated candidates | Query cost/invalidation proof; actual Region/Pool workload relevance |
| Rank 4: offline assembly | All 77 copied replacements assembled, passed Flugan round trips and Microsoft disassembly | Local tool, negative cases, original identity, permissions and rendering proof |
| Ranks 3 / 5–9 | SDK gates retained; bounded AMD/DXVK adapter checks pass in both architectures | Native product checks, built-in Wine 32-bit discrepancy, published SDK and original identity |

## Design choice

The minimal design retains pending work on `HackerContext`. The more flexible
design adds device-owned publication channels and resource epochs. The common
caller design keeps existing command dirty notifications and one context
publication operation. The preliminary implementation follows the common caller
design: it contains failure/retry policy while keeping current legal execution
points and CPU assignment semantics.

A failed upload retains a request, not a CPU pointer or stale copied value.
Retry consumes the newest complete CPU image. Initialization still publishes
zero values before [Constants]; successful uploads alone increment the existing
profiler counter. Texture extent/format mismatch fails without writing. No
frame-wide coalescing, global generation cache or new backend is introduced.
The [proposed decision](adr/0001-context-owned-iniparams-publication.md) records
why deferred-context recording cannot acknowledge another context's upload.

## Evidence so far

- The three approved portable publication behaviors pass with C++17, warnings as
  errors: failure/retry of newest values, missing-resource recreation and
  distinct publication images.
- The actual production D3D adapter passes in 64-bit Wine using an isolated prefix
  and offscreen textures: failed Map leaves prior GPU contents, retry uploads
  both rows, successive publication points retain their respective images, and
  recreation receives complete current contents. Two offscreen draws with no
  intervening readback retain different literal pixel values after one final
  readback. No swap chain or Present runs.
- Three inventory cases pass; the full existing Python runner suite plus them
  passes 11 checks. Exact `abc` SHA-256 is independently fixed in the fixture.
- Eleven deliberately broken variants are detected: lost pending state, missing
  acknowledgement, invented digest, invented original provenance, ignored
  recursive exclusion, missing include-cycle deduplication, counted literal-zero
  includes, acknowledging a failed Map, partial-image copying,
  inventory writes to an input and dropping the first draw's publication. The
  latter fails specifically on the final pixel values. Active sources were not
  mutated; restored isolated sources and clean executables pass afterward.
- The portable policy also passes AddressSanitizer and UndefinedBehaviorSanitizer.
- The approved adapter/draw checks also pass in 32- and 64-bit processes through
  Wine + copied DXVK DLLs on AMD RADV. The 32-bit policy and draw-only checks pass
  on built-in Wine, but the full adapter case fails on its second staging read.
  The old upload kernel fails identically; raw values are wrong before the CPU
  copy and next publication. The unchanged executable passes with DXVK. This
  remains an unresolved backend-dependent result, with no assertion weakened or
  production workaround applied. The [backend receipt](benchmarks/edo-pre-production-2026-11-16/backend-provenance.json)
  records the passing and failing routes separately.
- The same adapter fixture passes both architectures inside Steam Linux Runtime
  with Proton 11.0-2e, through its native DXVK build on AMD and its built-in D3D
  route. The Wine 10 failure does not reproduce there. Direct prefix invocations
  require the exact pass message as well as zero exit; the first normal launcher
  attempt lacked captured assertions and is excluded. See the
  [Proton receipt](benchmarks/edo-pre-production-2026-11-16/proton-provenance.json).
- The local installed inventory reproduces nine INI files, 77 assembly
  replacements and 503 TextureOverride sections. It is a candidate inventory,
  with no original game-build claim. A separately supplied unreached dev INI is
  recorded without counting its contents. All 87 recorded file digests were
  rechecked without changes. Adjacent texture mentions remain inspection hints,
  with unverified reuse legality.
- Two repeated upload-kernel probes change direction between runs; no speed
  improvement is established. All 77 copied replacements assembled and passed
  Flugan round trips and Microsoft disassembly, with a separately documented
  Microsoft text precision failure for the first probe. [Raw samples and provenance](benchmarks/edo-pre-production-2026-11-16/README.md)
  preserve both results and their limits. No shader assets or tool binaries are
  redistributed in those receipts.
- Pinned clang-format 23.1.1 and whitespace checks pass for the changed native
  source. Both changed project XML files and the workflow YAML parse; new
  runtime cases appear in the registry and workflow selection. Native checks
  remain outstanding.

Native runtime cases `iniparams-transition-retry`, `iniparams-command-retry` and
`entry-path` are registered and selected by the runtime workflow. Command retry
covers both empty and nonempty subsequent invocations. The existing mutation
runner can select these native cases with repeated `--case` arguments; all 70
current mutation anchors match exactly once. The four new native wiring
mutations remain unexecuted. A standalone native D3D
project and portable CI job are also wired. These Windows cases have not run.
The Windows desktop's SSH/SMB endpoints were unavailable; the mapped C: mount
reported no device. `~/bin/mount-lr-gamingpc.sh` is the existing mount helper if
the desktop becomes available. A MinGW syntax probe meets pre-existing MSVC/STL
compatibility errors; it is not a passing product build.

Local logs, inventories and executables are in ignored
`.quality/pre-production/`. Adapter results do not establish native Windows
MSVC/WARP behavior or full product, EDUP, vendor-wide or Proton game compatibility. There
is no FPS comparison or demonstrated performance benefit at this checkpoint.

## Experimental entry-path gate

The new early path resolves the command list under the existing configuration
lease and skips setup only for an empty list that is not runtime-populated. It
rechecks every invocation and still attempts a retained publication. Nonempty
execution receives the already-resolved list, avoiding a second proxy-chain
walk. This is a prototype pending measurement, not an accepted speed claim.

`benchmark-lists` uses a preallocated state and cannot measure this entry setup.
The new `benchmark-entries` mode uses public `RunCommandList` for empty, proxied
empty, dynamic empty and one-assignment workloads, with the D3D debug layer off.
A manual workflow can build an ancestor with the identical current harness,
then alternate A/B/B/A runs. It refuses a comparison with other production-file
changes. Native baseline selection and the shared harness digest are recorded;
no original-3Dmigoto or shipped-DLL claim follows from that comparison.
For this branch, select `e7d5439427fb72ef1461a7a949f7edf448b79f67` as the baseline:
it includes the publication repair and its documentation checkpoint, so the
candidate's only production-file difference is the entry-path experiment.

The context guard also defers variable transitions and pending post commands
when no wrapped context is available. Normal frame-action construction already
obtains that context; this fallback remains part of the preliminary native/lazy
startup verification gate. No extra GPU generation cache is needed for the
current implementation because successful-publication identities are not cached.

## Resume validation

1. After authorization to push this branch, run manual `ci.yml` with
   `zip_release=true` and `runtime-tests.yml` at its exact head. Use
   `mutation_checks=true`,
   `mutation_case_filter=iniparams-transition-retry,iniparams-command-retry,entry-path`,
   `mechanism_benchmarks=true`, and
   `entry_benchmark_baseline_ref=e7d5439427fb72ef1461a7a949f7edf448b79f67`.
   Retain source/compiler provenance and both correctness and timing artifacts.
2. Resolve native MSVC x64/Win32 and WARP failures before proposing integration.
   Interpret the known host Wine 10 32-bit discrepancy separately; it also
   occurs with the old kernel and does not reproduce in the tested Proton 11
   runtime. Keep the failing assertion intact.
3. Evaluate the empty-list path using the matched public-entry runs. Retain it
   only if it preserves outputs/profiling and saves material setup cost without
   an unacceptable nonempty-path regression. Current upload-kernel numbers
   establish no gain and cannot make this decision.
4. Additional cases for deferred-context ownership, texture-capacity mismatch,
   unknown include conditions and output no-clobber remain awaiting the user's
   test-case confirmation. Proposed offline-preparer negative cases also await
   confirmation; no new preparer has been implemented or shipped.
5. Then implement the bounded rank 4 preparer from the pack-wide input proof,
   or measure the one repeated-query example. Keep query effects, original
   provenance, asset permissions and future SDK services as explicit gates.
