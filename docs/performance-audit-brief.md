# EDHM performance audit — decision brief

**2026-11-12 · Source `34d92f27` · Elite Dangerous + EDHM**

**User-confirmed performance baseline:** the project's selectively ported XXMI
**v0.9.8** milestone. The vendor DLL is only a provenance check; newer XXMI
releases inform proposed ports.

The codebase has useful existing reductions in logging, matching, shader
processing and optional resource caching, alongside substantial compatibility
and safety work. The latest XXMI release is **v1.2.3**, beyond the locally ported
**v0.9.8** milestone. Original 3Dmigoto is covered against both stable **1.3.16**
and newest published **1.4.9 beta**. There are no runtime changes after local
**v0.1.3** in the audited checkout.
[Baseline evidence](audit-upstream-baselines.md).

**What was measured:** on this Linux host, isolating the local L2 region-hash
change reduced median map lookup for 1,024 equal-sized, 64-byte-spaced regions
from **216.26→8.15 ns for hits** and **420.63→15.51 ns for misses**. The current
map-template variant measured **8.82 / 16.50 ns**, using current L2 payloads
and `find_ptr` in all variants. This supports keeping the hasher
and safety checks. It is a synthetic CPU component result; **no Elite FPS gain
is established**. Region tracking is disabled in the shipped template.
[Method and all results](performance-audit.md#measurements-executed).

**What the Linux installation shows:** EDHM is present under the Steam Odyssey
product directory. Its log reports Proton/Wine 11.0, buffered non-debug logging
and **`dump_usage=1`**. Thirteen dated resource/frame-analysis captures exist,
but no paired frame-time measurements were found. The installed DLL differs
from the vendor LFS object by hash, so version 0.1.3 does not prove source
provenance. [Installation evidence](audit-data/linux-install-evidence.json).

**Recommended sequence:**

1. Establish a known Release baseline and repeatable Linux captures; measure
   disabling usage dumping as its own normal-play configuration change.
2. Remove the duplicate shader disassembly on patterned ShaderRegex misses and
   evaluate a safe disabled-hunting fast gate, as separate small changes.
3. Apply applicable correctness prerequisites, then isolate the newer parser
   compaction/lookup/expression optimizations with startup/reload timings.
4. Measure small tracking/view-reuse candidates; then port TextureOverride
   candidate caching with EDHM matching semantics preserved.
5. Port fixed adjacent-slot batching. Add cache-first `store` only if real
   readback stalls justify its write/memory cost. Conditional batching comes
   after fixed batching and must use the corrected upstream series.
6. Defer broad language/reflection/layout-cache redesign until a mod needs it.

**Required boundaries:** keep real input-layout pointers, classic hash-first
matching unless explicitly opted into dual matching, untracked-resource fallback,
safe cache invalidation and owned snapshots. Upstream candidate caching changes
matching semantics; earlier conditional batching was reverted after a crash.
Neither is suitable for blind import.
[Detailed order and sources](performance-audit.md#proposed-changes-and-order-for-elite--edhm).

**Decision confidence:** strong for the source mechanisms and the measured map
case; provisional for the adoption order. Individual game gains for other changes
remain unmeasured. Do not publish a total speedup or add percentages from these
mechanisms. Fresh matched A/B scene recordings are needed to complete that part
of the request. [Full audit and profiling protocol](performance-audit.md).
