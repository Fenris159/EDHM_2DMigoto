# Pre-SDK completion evidence

These receipts extend the [16 November follow-up](../edo-pre-production-follow-up-2026-11-16/README.md).
Historical files remain unchanged. Read the
[current preparation status](../../pre-sdk-preparation-status.md) for scope and
remaining external inputs. No game, FPS or Present-loop measurement ran.

## Source checkpoints and checks

- [Build CI at `a0c2b08c`](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/44337726280)
  passes ordinary Release/Zip Release, strict x64/Win32 builds, formatting/script
  analysis and shader compilation.
- [Runtime checks at `6c35c552`](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/44339031730)
  pass all 65 cases in x64/Win32, both proxy suites, the concrete upload/draw
  fixture and the new shader checker. The query case detected an intentionally
  constant filter result; restored production code passed. The
  [native mutation receipt](query-native-mutation.json) preserves source hashes.
- [Pack-acceptance collection at `2b03e6be`](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/44341473187)
  uses pinned public inputs and actual system D3D11 WARP shader objects on
  Windows. Both architecture receipts below report 77 accepted, zero rejected.
  Collection completion is independent of per-asset validity; inspect the data.

The initial new runtime run at `a0c2b08c` reached successful authored PS/VS
creation and malformed rejection, then incorrectly inherited the expected
nonzero process status at the PowerShell step's exit. The harness now captures
that child's status independently. The next query fixture used preset scope,
where resource operands are forbidden; it now supplies an actual command scope.
Neither correction changed the runtime parser or product DLL. New source after
`aeadf7a9` changes only tools, fixtures, workflows and documentation.

The local Python suite passes 23 cases. The
[six new inventory faults](semantic-mutations.json) were detected in isolated
copies, with clean checks after restoration. The
[checker's Wine receipt](checker-fixtures-wine.json) includes malformed PS/VS
rejection, unchanged files and sensitivity to falsely accepting malformed PS.
Native Windows outcomes remain separate from that Wine route.

## Paired expression measurement

The installed root's lines 733–734 assign `w49 = ps-t0`, then query `ps-t0`
again in a predicate. The experiment compares that pair with assigning `w49`
once and reading `w49` in the predicate. Both variants run in the same binary.
It does not apply a pack edit or change the runtime's query implementation.

The native case requires filter values 158 and 215 and predicates 1 and 0 after
rebinding. Its failed constant-result mutant proves the rebinding check is
sensitive. The benchmark includes stable and alternating-binding workloads.
Each of four processes records 11 paired samples per workload and diagnostic
mode, at 100,000 iterations per sample. Variant order alternates by sample;
each thread is pinned to one allowed CPU. Debug validation is disabled and no
upload, draw or Present occurs inside the timed loop. The estimator is the
median of process medians, with all samples retained.

| Profiling disabled workload | Repeated query, ns/pair | Assigned-row predicate, ns/pair | Change |
| --- | ---: | ---: | ---: |
| Stable binding | 264.59 | 139.23 | −47.4% |
| Rebinding each pair | 301.20 | 177.41 | −41.1% |

Profiling-SUMMARY controls are 519.69 to 265.15 ns for stable bindings and
556.70 to 304.66 ns for rebinding. They are diagnostic costs, not estimates for
profiling-disabled behavior. Exact predicate sums and final row values pass in
all 352 samples. SUMMARY matching counts are 200,000 versus 100,000 per sample;
disabled counters stay zero. This records less repeated work in this fixture.

[Raw metadata](queries/metadata.json), CSV/stderr beside it and
[recomputed summaries](query-summaries.json) retain host/source/executable hashes,
each process median and CSV hashes. No slow sample or process was removed.
The harness hash covers the Windows checkout's CRLF bytes; the Git blob uses LF.
The observed saving is about 124–125 ns per pair on this host, not a whole-DLL
or game-speed gain. Real invocation frequency was not measured. This uses
existing INI syntax and is not a fork-exclusive optimization or an A/B against
original 3Dmigoto.

**Decision:** offer the one local pack simplification for maintainer review.
Keep it within the same serial invocation with no intervening effects or
concurrent writer to the row. The inspected graph has only this one `w49`
assignment; that alone does not prove thread exclusivity. Do not cache
across `checktextureoverride`, rebinding, draw/configuration changes or unknown
commands; the other eight adjacent mentions cross effectful commands. A
general runtime cache has no demonstrated benefit/risk justification here.

## Semantic inventory

[The summary](semantic-inventory.json) describes the same 87 unchanged installed
inputs as the earlier receipt. It is a lexical candidate inventory, not an
interpreter or execution trace. It records 858 literal assignments, 32
expressions, 1,684 control directives, 2,344 resource-write candidates, 20
effectful commands and 72 unclassified directives. No synchronous store command
was found in this graph. Of 823 unconditional Constants literal candidates,
816 target row components; none is claimed to be the final effective value.
Unknown include conditions and runtime conditions remain uncertain. Signed
zero spelling is retained. Every classified directive remains a conservative
reuse boundary; this data never authorizes caching.

## Public source and native shader acceptance

The two pinned EDHM archives reproduce all 77 installed replacement digests.
The [source investigation](shader-investigation.json) records exact upstream
FSS bytes and the bounded, unsuccessful search for its original game container.
The committed fixture recipes identify every source member and pin the helper
archive and both tool members. No shader, prepared DXBC, compiler or archive is
included in these receipts or workflow artifacts.

| Route | Accepted objects | Rejected objects | Rendered output |
| --- | ---: | ---: | --- |
| Native Windows x64, system D3D11 WARP | 77 | 0 | Not tested |
| Native Windows Win32, system D3D11 WARP | 77 | 0 | Not tested |
| Host Wine 10 built-in route, complete public-source collector | 77 | 0 | Not tested |
| Host Wine 10 built-in route, maintained checker, strict subset | 76 | 0 | Not tested |

[x64 receipt](pack-native-x64.json), [Win32 receipt](pack-native-Win32.json) and
[earlier selected-subset Wine receipt](checker-selected-batch-wine.json) record
the independent outcomes. Windows bytecode digests match across architectures;
the 76 strict outputs also match the prior independent Wine preparation.
All source/bytecode inputs remain unchanged.
The later [full Wine collection](pack-wine-observations.json) also reports all
77 accepted with the same bytecode digests as Windows; its requested-WARP route
does not establish Microsoft WARP or hardware coverage.

The strict preparer still rejects the FSS asset's missing declared input
signature metadata. Its unchanged helper output was probed separately and
accepted on both Windows routes. Thus that source inconsistency is not a
demonstrated native creation failure. Creation success also does not prove the
intended position input, temporary usage, linkage, bindings or pixels. Preserve
the conservative preparation exclusion until the relevant metadata/host
contract and intended output are settled. See the
[full shader report](../../pre-sdk-shader-evidence-2026-11-18.md).

Native D3D12 acceptance, original shader identity, texture tags and future-host
integration remain distinct gates. Public-source equality is not original
identity or permission to distribute an adapted pack.
