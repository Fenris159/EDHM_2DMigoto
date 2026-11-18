# Preparation before the edo-next SDK

Updated 18 November 2026 on `pre-production-edo-readiness`. This closes the
SDK-independent work selected by the [main plan](edo-next-readiness-2026-11-16.md).
It does not certify a native plugin, installed-game behavior or redistribution.
The earlier percentage estimate was engineering judgment, not a task-count or
calendar estimate; use the explicit completion gates below instead.

The work added in this follow-up changes inventory tooling, off-game checks,
measurement harnesses and documentation. It makes no additional change to the
product DLL's runtime source or installed EDHM/EDUP files.

## Work within this repository

| Preparation item | Deliverable and decision | Completion gate |
| --- | --- | --- |
| Failed uploads and publication ownership | Context-owned requests, newest-value retry and independent deferred recording | Existing native output/mutation checks pass; implementation remains preliminary |
| Empty command entries | Per-invocation shortcut with no retained empty classification | Matched synthetic receipts and native regressions retained; no game-speed claim |
| Inventory and supported-value scope | Literal/expression assignments, dependencies, runtime conditions, readbacks, resource writes and unclassified directives | Seven inventory cases, six new sensitivity faults, exact unchanged inputs |
| Repeated-query candidate | Compare the installed pair's second query with its already-assigned row | Native exact-value/rebinding case, mutation sensitivity and paired CPU measurements |
| Settings/editor/package contract | Separate immutable defaults, editable generations, runtime state and native-plugin feed | Written proposal with current editor conversion evidence and explicit unresolved SDK fields |
| Offline bytecode preparation | Pinned immutable inputs, strict manifests and separate object-acceptance observations | Preparer sensitivity checks, authored fixtures and all 77 objects accepted by Windows WARP on both architectures |
| FSS metadata/original identity | Pin upstream bytes, classify both source inconsistencies and inspect existing provenance | Evidence investigation and retrieval recipe complete; missing original bytes remain external |
| Conditional refactors | Defer Region/Pool changes and generic value/settings extraction | No current Region/Pool workload or concrete second consumer demonstrates their value |
| Host handoff | Specify acceptance outcomes and the exact missing evidence | This document, the settings proposal and the main plan's host questions are reviewable without invented SDK types |

The [implementation report](pre-production-edo-readiness.md) retains the earlier
runtime checkpoints. New receipts belong under
[`pre-sdk-completion-2026-11-18`](benchmarks/pre-sdk-completion-2026-11-18/README.md),
separate from the immutable 16 November measurements.

## What the source inventory establishes

The inspected graph still has 87 recorded files and unchanged input digests.
It contains 858 literal assignments and 32 expression assignments, alongside
1,684 control directives, 2,344 resource-write candidates, 20 effectful commands
and 72 unclassified directives. These are lexical source counts, not execution
frequency, allocations or runtime costs. No synchronous `store` command was
found in this graph. The future importer must preserve unsupported behavior
as an exclusion; it must not flatten that behavior into constants.

Of the literal assignments, 823 occur as unconditional Constants candidates;
816 are row assignments, with duplicate targets possible. They are not 816 proven final values:
include order, duplicate assignments and later commands still matter. The
tool preserves signed-zero spelling, source lines and conditional uncertainty.
It never chooses a theme or executes the INI.

The [settings/package proposal](edo-settings-package-contract.md) uses existing
EDHM_UI conversion and a completed-batch handoff. Packed JSON colours require
the editor's gamma conversion; they are not raw parameter floats. Mtime-only
retry notifications do not define new settings generations. These decisions
can be reviewed now; actual editor/host integration needs those projects'
agreement and a real consumer.

## Query and shader decisions

The one assignment-then-query pair is a bounded experiment. The eight other
adjacent pairs cross `checktextureoverride`, which can run matched command
lists; they do not justify a general cache. The fixture compares exact filter
values and predicates before and after rebinding. Its timing loop measures the
two expressions in the same binary, with no upload, draw or Present. Retain the
raw measurements and the rebinding control when reviewing a pack edit. With
profiling disabled, the paired operation's median fell from 264.59 to 139.23 ns
in the stable fixture (47.4%) and from 301.20 to 177.41 ns with rebinding (41.1%).
Those are four-process synthetic expression results, not a game-speed claim.
Recommend the one local expression simplification for a maintainer-reviewed pack
change within a serial invocation, with no intervening effect or concurrent
row writer; the lexical graph alone does not prove thread exclusivity. Do not
add a general runtime cache for this isolated opportunity. The
installed configuration remains unchanged.

The [shader evidence report](pre-sdk-shader-evidence-2026-11-18.md) ties the
rejected FSS source to exact upstream bytes. It also identifies its undeclared
temporary-register use. All 77 helper outputs were accepted by a private Wine
creation probe; that does not prove native rendering. The maintained checker
accepts all 76 strictly prepared inputs on the tested Wine route. Native
Windows checks use authored PS/VS fixtures and malformed inputs, independent
of the pack's permissions and original-byte provenance.

The public source investigation also reproduces all 77 exact source digests
from two pinned archives. An optional CI lane fetches them into temporary
storage, prepares the strict subset, probes actual shader objects, then removes
the assets/tools. It retains acceptance/rejection observations and hashes only.
Successful collection does not mean every asset was accepted; the receipt's
per-shader results and `all_assets_accepted` field govern that claim.
The collected Windows x64 and Win32 results each report 77 accepted and zero
rejected. Both routes produce identical bytecode digests; the 76 strict outputs
also match the earlier independent Wine preparation. This closes the available
native object-creation gate, while original identity and intended rendering
remain separate external inputs.

Keep the preparer's strict policy. Do not invent a signature, silently alter
an optional add-on, or equate shader-object creation with correct stage
linkage/resources/output. A future plugin proof can begin with an authored
sample while the pack's evidence and permissions are resolved.

## External inputs still required

| Input | What exists now | What would close it |
| --- | --- | --- |
| Original shader/build provenance | Current version/executable digests; replacement identity; no recovered matching original container | Existing original export/capture with raw bytes, stage, capture-build/executable provenance and independently verified legacy key |
| Intended FSS metadata and output | Pinned inherited source inconsistencies; bounded helper/creation results | Author-supplied correction or authoritative intended linkage/output evidence, then independent validation on a copied asset |
| Asset adaptation/distribution rights | Pinned maintainer-stated terms and specific exceptions | Applicable permission covering the intended native package; local possession/tool success is insufficient evidence |
| Editor and publisher agreement | Concrete generation/path/update proposal | Accepted editor handoff, plugin identity, feed ownership and signing/rollback policy |
| Native host and SDK | Requirements and a separate-plugin plan | Versioned headers/checker/sample, runnable host, original identity and binding/ownership/tag contracts |
| Wider device/backend validation | Existing bounded Windows/Wine/Proton fixtures | Available NVIDIA/Intel and other required routes with the same explicit correctness checks |

These entries are not completed compatibility claims. The retrieval recipe
uses existing exports; obtaining a new game capture remains outside this
session. No messages or permission requests were sent to other maintainers.

## Next entry conditions

1. Review the preliminary branch and retain its exact-source CI evidence before
   a separately authorized integration into develop.
2. On receipt of originals or an author correction, verify copies against the
   documented identity/signature gates. Keep source and prepared hashes distinct.
3. On receipt of a runnable SDK preview, pin it and first prove an authored
   minimal plugin: registration, copied constants, teardown and correct output.
   Then integrate a permitted, identity-proven HUD replacement.
4. Add texture-filtered panels only when the host implements the required tag,
   matching, update and invalidation semantics. Scene effects and custom
   resources retain their later service gates.

No SDK-specific module, guessed host manifest, automatic updater or broad
renderer abstraction is needed to finish the preparation decisions above.
