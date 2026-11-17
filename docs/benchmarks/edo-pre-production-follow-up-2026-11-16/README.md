# Pre-production follow-up evidence

The user approved the additional regression cases, pushing the branch, native
CI and the bounded offline preparer after the initial three-hour session.
These receipts extend the [initial evidence](../edo-pre-production-2026-11-16/README.md)
and [implementation status](../../pre-production-edo-readiness.md). The original
receipts are unchanged. Dates in workflow logs are UTC.

## Native checks

[CI at `6b490edc`](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43998532098)
and its [runtime workflow](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/43998529957)
passed. [CI at `d9e700c9`](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/44000753321)
and its [runtime workflow](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/44000755930)
also passed. These include ordinary Release/Zip Release checks, strict x64/Win32
builds, 64 native runtime cases, concrete WARP upload/draw checks and both
product DLL proxy suites. The latter use pinned off-game EDUP/helper/reload
fixtures; they do not run Elite Dangerous.

The x64 sensitivity run detected five faults: shared context acknowledgement,
lost nonempty/empty retries, retained empty classification and lost final
transition retry. Restored production behavior passed. [Checkpoint metadata](ci-checkpoints.json)
links the initial failed benchmark harness to its correction: a null namespace
was passed to a global-variable parser. That failure did not affect the already
passing product checks. Later runs retain benchmark stderr and native exit codes.

The local suite passes 21 Python cases. The [16 publication/inventory/adapter
faults](publication-inventory-mutations.json) and [10 preparer faults](preparer-mutations.json)
were detected in isolated copies; restored checks passed. The preparer's content
checks include same-size file changes with fixed filesystem timestamps.

## Matched public-entry timing

Each source pair builds the exact same harness with the same MSVC toolchain
and flags in one Windows job. The ancestor is `e7d54394`, which already contains
the publication repair. CommandList.cpp is the sole production-file difference.
The debug layer is off, configuration ownership stays active and literal command
results are checked. Each process records 11 samples of 200,000 invocations per
workload/mode. The first two series use A/B/B/A separate processes.

Raw data and executable/harness hashes:
[initial prototype](entry-6b490edc/metadata.json),
[refined prototype](entry-d9e700c9/metadata.json), with CSV/stderr beside each.
The estimator below averages the two process medians for each variant, then
computes `(candidate / baseline - 1) × 100`; negative means less CPU time.

| Source / profiling disabled workload | Baseline ns/call | Candidate ns/call | Change |
| --- | ---: | ---: | ---: |
| `6b490edc`: static empty | 27.05 | 12.42 | −54.1% |
| `6b490edc`: proxied empty | 28.05 | 13.05 | −53.5% |
| `6b490edc`: dynamic empty, excluded from shortcut | 26.99 | 27.74 | +2.8% |
| `6b490edc`: one assignment | 49.41 | 49.20 | −0.4% |
| `d9e700c9`: static empty | 26.99 | 12.42 | −54.0% |
| `d9e700c9`: proxied empty | 28.17 | 13.04 | −53.7% |
| `d9e700c9`: dynamic empty, rechecked per invocation | 26.95 | 12.42 | −53.9% |
| `d9e700c9`: one assignment | 49.18 | 70.42 | +43.2% (variable) |

The last row is retained as an unresolved timing result. Its candidate process
medians are 47.980 and 92.852 ns, versus baseline 49.607 and 48.761 ns. The same
binary produced both candidate results. Profiling-SUMMARY assignment controls
were near baseline (96.69 versus 97.23 ns). This does not justify dropping the
slow process or claiming unchanged nonempty cost. The controlled repeat pins
the benchmark thread to one allowed logical CPU and uses A/B/B/A/B/A/A/B
processes; its results are assessed separately below.

The [controlled runtime workflow](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/44002493520)
passed at `6f2e69e9`. All eight process stderr logs record affinity mask 1.
[Raw controlled data](entry-6f2e69e9/metadata.json) and
[recomputed summaries for all three series](entry-summaries.json) retain every
sample and process median. For profiling disabled:

| Controlled workload | Mean of baseline process medians, ns | Mean of candidate process medians, ns | Change |
| --- | ---: | ---: | ---: |
| Static empty | 21.34 | 10.13 | −52.5% |
| Proxied empty | 21.88 | 10.63 | −51.4% |
| Dynamic empty | 21.35 | 10.09 | −52.7% |
| One assignment | 45.10 | 43.15 | −4.3% (variable) |

The last mean includes a slower baseline process, so it does not establish a
nonempty speed improvement. The medians of process medians are 42.84 ns baseline
and 43.17 ns candidate, a +0.8% difference. Baseline process medians range from
41.83 to 52.89 ns; candidate medians range from 42.33 to 43.90 ns. The earlier
large candidate outlier did not recur. Profiling controls remain variable too.
This supports retaining the small empty-entry prototype for review, with a
narrow synthetic claim and explicit nonempty variability; it is not a promise
of identical timing across hosts or a measured game benefit.

The empty-path saving is roughly 11–15 ns per synthetic invocation. Call frequency
in the game was not measured, and the optimizer can already remove some empty
work. No FPS, Present-loop, original-3Dmigoto, whole-DLL or whole-game speedup
follows from these numbers. The initial upload-kernel repeats established no gain.

## Offline preparation

The [preparer receipt](preparer-provenance.json) records pinned tool digests,
all 87 checked input files, the selected batch recipe and output hashes. The
explicit 76-asset subset prepared and passed read-only verification. Every output
hash matches the earlier independent helper assembly. No tool binaries, game
assets or prepared DXBC files are redistributed here.

The full 77-asset batch was rejected without a success manifest. In
`ShaderFixes/ea5390a2f20c2368-ps.txt`, the source declares
`dcl_input_ps_siv linear noperspective v1.y, position`; its input signature
comments contain only register 0's TEXCOORD entry. The independently prepared
ISGN likewise lacks register 1. The helper-only round trip did not expose that
gap. The new coverage policy refuses to claim complete preparation metadata.
This does not prove that the current D3D11 runtime fails; system-value handling
and any retained original signatures require separate native/original evidence.
No installed file was edited and no signature was invented.

The [real helper contract checks](real-helper-contract.json) also prepared an
authored minimal shader with frozen digest and rejected an unsupported
instruction through the actual helper, preserving its input and leaving no
success manifest. The eight preparer unit cases substitute only the external
process, exercising stale inputs/tools, architecture mismatches, signatures,
container bounds, process errors and refusal to overwrite existing outputs.

Read the [tool guide](../../offline-dxbc-preparer.md) before use. The output is
structural preparation with unchanged bindings, including t120. It is not a
runtime cache, a distributable mod/plugin, native rendered-output validation or
future SDK compatibility proof.
