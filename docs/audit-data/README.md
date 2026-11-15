# Performance audit evidence

These artifacts accompany the [2026-11-12 audit](../performance-audit.md).
They describe pinned revisions and one measurement session, not a live dashboard.

| File | Meaning |
|---|---|
| `changed-files.csv` | Git numstat for original stable→local, original beta→local, XXMI v0.9.8→local, local v0.1.3→local, XXMI v0.9.8→v1.2.3, and XXMI v1.2.3→local. Includes removed/vendor files; line counts are not performance gains. `-` means binary. |
| `linux-install-evidence.json` | Scoped read-only inspection of the installed game, effective log settings, hashes and environment/tool limitations. Does not include private account/session logs. |
| `region-cache-environment.json` | Exact source/extraction/harness hashes, compiler, flags, affinity and workload protocol. All map variants use the current payload and `find_ptr` to isolate mechanisms. |
| `region-cache-raw.csv` | Every observed batch duration normalized to nanoseconds/lookup, output checksum and initial-bucket count. 15 samples × 5 key sets × 2 lookup outcomes × 3 variants. |
| `region-cache-summary.csv` | Median/min/max of the 15 batches per case. These are within-session observations, not cross-run confidence intervals. |

The raw variant label `xxmi_0.9.8` means the extracted upstream map and hasher
with a controlled current payload; it does not mean a complete historical DLL
or historical cache-entry layout. `hash_only` changes the hasher alone;
`edhm_head` uses the current map and hasher. Construction, insertions, range
validation and GPU work are outside the timed region.

Reproduction requires Git objects for the pinned commits plus Python 3 and g++:

```bash
python3 scripts/benchmark-region-cache.py --cpu 2 --output .quality/audit-repeat
```

Run from the repository root. Choose an available CPU or omit `--cpu` on a host
without that affinity. The harness checks variant checksums before emitting
observations and records its own SHA-256. Keep repeated sessions in separate
output directories. The original `uint64_t`/`find(out)` development run was
superseded by the current-payload/`find_ptr` run saved here.

Existing behavior validation executed separately:

```bash
x86_64-w64-mingw32-g++ -std=c++17 -O2 -Wall -Wextra -Werror -static \
  AdvancedHuntingTests/AdvancedHuntingTests.cpp DirectX11/AdvancedHuntingContext.cpp \
  -o /tmp/edhm-audit-profile/AdvancedHuntingTests.exe
WINEDEBUG=-all wine /tmp/edhm-audit-profile/AdvancedHuntingTests.exe
```

Observed output: `Advanced hunting tests passed`; exit code 0. This is an
existing CPU behavior check under MinGW/Wine, not an MSVC build, live-game
integration check or frame-performance result.
