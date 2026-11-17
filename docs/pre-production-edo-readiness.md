# Edo-next preparation: implementation status

Branch: `pre-production-edo-readiness`, based on `develop` at
`4ae925945879a897309992f58db6cc752f22289d`. The initial three-hour session ran
16 November 2026, 18:59–21:59 UTC. The user then approved the additional cases,
pushing the branch, native CI and a bounded offline preparer. Work remains
preliminary: no merge, release, version bump or game installation was performed.
The original checkout and release branches are untouched.

The [main plan](edo-next-readiness-2026-11-16.md) remains the work order.
[Initial receipts](benchmarks/edo-pre-production-2026-11-16/README.md) are historical;
[follow-up receipts](benchmarks/edo-pre-production-follow-up-2026-11-16/README.md)
record the later native checks, measurements and preparer findings.

## Implemented checkpoints

| Plan item | Result | Remaining gate |
| --- | --- | --- |
| Rank 1 / 2A: publication safety and ownership | One context-owned pending request; checked complete-image upload shared by commands, transitions and initialization | Product/game validation remains separate from native fixture results |
| Rank 2: passive inventory | Include graph, exact digests, explicit unknown provenance, candidate rules/rows and unreached-file recording | Original game containers/build provenance and semantic-cost inventory |
| 2C: empty-list path | Per-invocation resolved-vector check; native regressions and controlled measurements pass | Narrow synthetic claim; nonempty process variability and real invocation frequency remain limits |
| Rank 4: offline preparation | Pinned helper/compiler, separate immutable staging, signature coverage, validation and content manifests | One installed asset lacks metadata required by the conservative policy; native shader/render/SDK proof and asset permissions |
| 2B / 2D / 2E | Remain gated | One repeated-read example needs cost proof; eight adjacent pairs cross effects; current pack has no Region/Pool workload |
| Ranks 3 / 5–9 | Existing contract and SDK gates retained; bounded backend adapter evidence recorded | Published host/SDK and original identity where required |

## Publication behavior

A failed upload retains a request on its context, without retaining a CPU
pointer or stale image. Retry consumes the newest complete CPU values.
Initialization still publishes zero values before [Constants], and transition
request frequency and legal upload points are retained. Successful uploads alone
increment the existing counter. Empty configurations remain no-ops; missing or
wrong-capacity resources preserve pending work and prior contents.

The [proposed ownership decision](adr/0001-context-owned-iniparams-publication.md)
keeps deferred recording separate from immediate acknowledgement. The new native
case records value 4 on a deferred context, retries immediate value 7, then
executes the recorded image and requires value 4. The approved checks also cover
final-transition retirement, empty/nonempty retry points, two draws with distinct
images, later list population, proxy retargeting and profiler outcomes.

There is no frame-wide coalescing, global generation cache, new renderer or
SDK-specific runtime path. The missing-context fallback defers the queued
transition tick and post commands; normal frame-action construction already
obtains the wrapped context.

## Validation

- The first native checkpoint (`eee51bd5`) passed all 64 runtime cases in x64
  and Win32, concrete WARP adapter checks and both product proxy suites. Its
  benchmark harness failed because a global-variable parser received a null
  namespace. `6b490edc` fixed that caller and retained stderr/exit diagnostics.
- [CI at `6b490edc`](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43998532098)
  and [runtime checks](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43998529957)
  passed, including five deliberate native wiring faults and the first matched
  public-entry measurement. The public DLL proxy/EDUP/helper/reload checks ran
  off-game; they are not installed-game smoke tests.
- [CI at `d9e700c9`](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/44000753321)
  and [runtime checks](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/44000755930)
  also passed after extending the empty path to rechecked dynamic lists and
  adding the offline preparer. Newer documentation/test refinements are recorded
  separately from those pinned source checkpoints.
- The portable publication policy covers four behaviors. Local Python checks
  pass 21 cases, including eight preparer cases. Same-size asset/tool changes
  are tested with fixed filesystem timestamps. Sixteen policy/inventory/adapter
  faults and ten preparer faults were detected in isolated copies; clean checks
  passed after restoration. Active production files were not mutated locally.
- The concrete adapter/draw fixture passes in 32- and 64-bit Proton 11 inside
  Steam Linux Runtime, through native DXVK on AMD and built-in D3D. The host
  Wine 10 built-in 32-bit repeated-readback discrepancy is preserved: it also
  occurs with the old upload kernel and does not reproduce on the tested DXVK,
  Proton or native Windows routes. Its exact cause remains unresolved.

Native results establish their fixture/source configuration. They do not prove
NVIDIA/Intel coverage, installed EDHM/EDUP game behavior, D3D12 rendering or
future edo-next compatibility. No FPS or live-game monitoring was used.

## Measurements and inventory findings

The initial upload-kernel repeats changed direction and established no gain.
The matched entry runs use the same harness/compiler for ancestor
`e7d5439427fb72ef1461a7a949f7edf448b79f67` and the candidate; CommandList.cpp is
the sole production-file difference. Static/proxied empty entry cost fell about
54% in the first run. Excluding dynamic lists added about 3% to that control;
the refined path now rechecks every empty vector without retaining a classification.

The refined run also cut dynamic-empty cost by about 54%, but one unprofiled
nonempty candidate process was a large outlier. Both candidate processes and
the full raw samples are retained. The controlled repeat pins the benchmark
thread and uses A/B/B/A/B/A/A/B processes. It confirms about 51–53% lower
empty-entry CPU cost. The large candidate outlier does not recur; nonempty
medians differ by about +0.8%, with visible process variation. The
[controlled workflow](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/44002493520)
passes at `6f2e69e9`. Retain the prototype for review with that narrow claim;
these nanoseconds per synthetic call do not establish a game or DLL-wide gain.

The passive pack inventory contains nine candidate INIs, 77 assembly replacements
and 503 TextureOverride sections, plus one explicitly supplied unreached dev
INI whose contents do not affect counts. All 87 file digests remained unchanged.
Of nine adjacent root texture mentions, eight cross `checktextureoverride` and
one is assignment then query. The count does not justify a general query cache.

## Offline preparer and metadata gap

The [preparer](offline-dxbc-preparer.md) never overwrites an existing output.
It pins both PE tools, rejects architecture mismatches, validates bounded DXBC
structure and declared signature coverage, requires helper round-trip and
Microsoft-disassembly success, then publishes a content-based selected-batch
manifest. Read-only verification needs no helper execution. Source/tool changes
invalidate results; original game identity remains explicitly unverified.

The earlier helper-only experiment assembled and round-tripped all 77 assets.
The stricter preparer correctly refuses the full batch at
`ea5390a2f20c2368-ps.txt`: `dcl_input_ps_siv ... v1.y, position` has no input
signature entry in its comments. This is a preparation metadata gap under the
conservative policy, not proof the current D3D11 mod fails. No signature was
invented and no installed asset was changed. The full-batch failure left no
successful manifest. An explicitly selected 76-asset subset prepared and passed
verification; every output digest matched the prior independent assembly.

The real helper also accepted the authored minimal fixture with its frozen DXBC
digest and rejected a parse-error fixture without a success manifest. Prepared
files keep current D3D11 bindings, including t120. They are not a runtime cache,
a distributable EDHM pack or a native edo-next plugin.

## Next work

1. Retain the publication repair and empty-entry prototype for review, with the
   native regression gates and explicit measurement limits. Keep the offline
   preparer independent of the runtime and retain its strict signature policy.
2. Preserve the new regression gates and native CI results when preparing a PR
   to develop. No merge or release is authorized by this follow-up.
3. Obtain original signature/build evidence for the metadata-gap asset before
   proposing a pack correction or host-ABI transform. Obtain asset permission
   before distributing prepared replacements.
4. Measure the one repeated-query example before introducing reuse policy.
   Keep Region/Pool work conditional on an actual workload.
5. Leave SDK types, settings/package integration, shader relocation and native
   host rendering proofs gated by the published host contract and SDK.
