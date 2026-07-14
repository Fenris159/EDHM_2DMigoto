# EDHM_2DMigoto — `develop` branch audit

**Date:** 2026-07-14
**Branch audited:** `develop` @ `6fba6125` (package `v0.1.3-alpha.2`, DLL `0.1.3`)
**Base of comparison:** `origin/xxmi-base` (clean XXMI-Libs-Package mirror)
**Scope:** The EDHM-specific delta applied on top of XXMI — i.e. the changes that
implement this repo's stated goal. The large inherited 3Dmigoto/XXMI/DirectXTK/pcre2
codebase was *not* re-audited except where the delta touches it.

---

## 1. Stated intent (from README, docs, CHANGELOG, and `patches/0001-edhm-xxmi-compat.md`)

Ship a focused, XXMI-derived `d3d11.dll` that restores **classic 3Dmigoto 1.4.5
behaviour** where XXMI ("2Dmigoto") diverged, so that the Elite Dangerous HUD Mod
(EDHM) works unmodified. The concrete goals are:

1. Load `[Include]` files on first parse (EDHM ships hundreds of TextureOverrides via includes).
2. Restore "hash TextureOverride wins alone" precedence (correct HUD colour selection).
3. Implement `auto_refresh_file_to_monitor` (theme reload without F11).
4. Stop crashing on location load by never handing the game a COM-wrapped input layout.
5. `CreateRasterizerState1` scissor parity.
6. Live-play stability: quiet hot-path logging, safe crash handler, versioning cleanup.

**Overall verdict:** The delta is well-targeted, internally consistent, and correctly
implements every stated goal. No defect was found that would prevent the branch from
achieving its purpose. Several *latent* XXMI bugs were actually fixed along the way.
Findings below are low-severity robustness/consistency notes, plus a catalogue of the
correctness wins for traceability.

---

## 2. Correctness wins verified (goal-critical)

| # | Area | Finding |
|---|------|---------|
| W1 | **InputLayout side-car** (`HackerInputLayout.*`, `HackerDevice::CreateInputLayout`, `HackerContext`) | The COM-ownership model is now **correct**. `CreateInputLayout` keeps `*ppInputLayout` as the *real* `ID3D11InputLayout`, attaches the wrapper as a side-car via `SetPrivateDataInterface`, then `Release()`s its construction ref — leaving exactly one ref owned by the original layout's private data. Lifetime is tied to the real object; no leak, no double-free. Destructor is empty (does not touch the now-dangling `mOrigLayout`). `Release()` no longer releases the game's create-ref (the previous double-free). Verified the wrapper is **never** assigned to `*ppInputLayout` anywhere and no `static_cast<HackerInputLayout*>` on a game pointer remains. This directly resolves the reported `ACCESS_VIOLATION` in `System32\d3d11.dll`. |
| W2 | **Hash vs fuzzy precedence** (`ResourceHash.cpp` `find_texture_overrides_for_resource`) | Classic early-return restored: `if (!matches->empty() && !G->fuzzy_match_alongside_hash) return;`. XXMI dual-match kept behind an opt-in flag. Matches the documented EDHM requirement. |
| W3 | **Early includes** (`IniHandler.cpp`, `globals.h`) | `skip_early_includes_load` default flipped to `false` in both the runtime default and the `Globals` constructor; template `Dependencies/d3dx.ini` sets `skip_early_includes_load = 0`. Consistent across all three sites. |
| W4 | **auto_refresh_file_to_monitor** (`IniHandler.cpp` parse, `HackerDXGI.cpp` poll) | mtime polled at ≤ 2.0 s from `RunFrameActions`; on change sets `gReloadConfigPending`. A second `if (gReloadConfigPending) ReloadConfig(...)` after the poll lets the reload land the same frame. `ReloadConfig` clears the flag first thing, so no infinite reload loop. |
| W5 | **Present1 logging** (`HackerDXGI.cpp`) | Removed the per-call `gLogDebug = true/false` toggle. This eliminated both a per-frame verbose-log storm *and* an unsynchronised write to the global `gLogDebug` from the render thread (a latent data race). |
| W6 | **Hunting empty-set guards** (`HackerDXGI.cpp` `Present`) | Added `!empty()` guards before `size() - 1` / `*std::prev(end())`. Previously, an empty visited set produced an unsigned wrap (`size()-1`) and UB deref. |
| W7 | **Crash handler resume** (`util.cpp`) | Default return changed `EXCEPTION_CONTINUE_EXECUTION → EXCEPTION_EXECUTE_HANDLER`. Since the handler is installed via `SetUnhandledExceptionFilter`, the old value re-ran the faulting instruction (infinite fault loop); the new value lets the process terminate after writing diagnostics. Interactive `crash=2` still offers explicit continue. Correct. |
| W8 | **CreateRasterizerState1 scissor parity** (`HackerDevice.cpp`) | `rasterizer_disable_scissor` now honoured on the DESC1 path, matching the legacy DESC path. |
| W9 | **Latent XXMI bug fixed** (`ResourceHash.cpp` `RegionHashesCache`) | `data_size` was previously an **uninitialised** member and `Initialize()` never assigned it, so the `data_size != buffer_size` guard was unreliable. Now default-initialised to `0` and assigned in `Initialize()`. Also added `if (end <= start) return;` and 64-bit-safe `size_t` page math with `UINT_MAX` clamping in `Invalidate`. Genuine correctness improvement, independent of EDHM. |
| W10 | **CreateUnorderedAccessView resize** (`HackerDevice.cpp`) | Refactor from signed `-1` sentinel to unsigned with an underflow guard (`requested > FirstElement`). Behaviourally equivalent to the original max-selection (`max(rawᵢ) − FirstElement == max(rawᵢ − FirstElement)`) but no longer underflows. |

Version identity (`version.h` = `0.1.3`, root `VERSION` = `0.1.3-alpha.2`,
product name `EDHM_2DMigoto`) is consistent with the documented scheme
(DLL file version tracks `MAJOR.MINOR.PATCH`; package tag carries the pre-release
suffix). Build graph change (dropping the `D3D_Shaders` project reference and
compiling its sources directly) matches `docs/layout.md` and is present in the
`.vcxproj`.

---

## 3. Findings (low severity)

### L1 — `auto_refresh_file_to_monitor` path resolution is not fully defensive
`DirectX11/IniHandler.cpp` (auto_refresh parse block):

```c
if (GetModuleFileName(migoto_handle, migoto_path, MAX_PATH)) {
    wcsrchr(migoto_path, L'\\')[1] = 0;      // (a)
    if (setting[0] && setting[1] == L':') {  // (b)
```

- **(a)** `wcsrchr(...)` can return `NULL`; `[1] = 0` would then dereference null.
  In practice `GetModuleFileName` always yields a path containing `\`, so this is
  effectively unreachable, but it is an unguarded null-deref. `GetModuleFileName`
  truncation (return value == `MAX_PATH`) is also not checked.
- **(b)** The "absolute path" test only recognises a drive-letter (`X:`). A UNC path
  (`\\server\share\...`) would be treated as relative and concatenated onto the DLL
  directory, producing a malformed path (the watch then silently never fires).

**Impact:** Negligible for the EDHM use case (a relative `EDHM-ini/...` path).
**Suggestion:** Guard the `wcsrchr` result; treat a leading `\\` (UNC) or `\`/`/` as absolute.

### L2 — "Ignorable exception" path uses `EXCEPTION_CONTINUE_SEARCH` for continuable exceptions
`util.cpp` `migoto_exception_filter` returns `EXCEPTION_CONTINUE_SEARCH` for the
non-fatal codes (`0x87A` debug-layer break, `0x406D1388` SetThreadName, `DBG_PRINTEXCEPTION_*`).
Because the filter is the *top-level* `SetUnhandledExceptionFilter`, `CONTINUE_SEARCH`
only truly "ignores" the exception when a debugger or an outer SEH handler exists;
with no debugger attached it falls through to default process termination. For a genuinely
*continuable* exception the semantically-correct "resume and ignore" return is
`EXCEPTION_CONTINUE_EXECUTION`.

**Impact:** Low. These codes almost always have their own `__try/__except` or reach the
filter only under a debugger (the Debug-build / debug-runtime scenario the fix targets),
and a Release `crash=1` play session does not raise them. Worth a comment or a switch to
`EXCEPTION_CONTINUE_EXECUTION` if you want hard guarantees.

### L3 — Log rotation is best-effort
`IniHandler.cpp` rotates via `DeleteFileW(bak); MoveFileW(log, bak);` before reopen.
If a stale handle still holds `d3d11_log.txt`, `MoveFileW` fails silently and the log
keeps growing past `max_size_mb`. Acceptable for the intended single-instance use;
noting for completeness.

### L4 — Two `ReloadConfig` calls can run in one frame
The added second `if (gReloadConfigPending) ReloadConfig(...)` means a frame in which
*both* a manual reload trigger (e.g. F10) and an auto-refresh mtime change occur will
run two full, critical-section-guarded reloads back-to-back. Correct, but a wasted heavy
reload in a rare race. Optional micro-optimisation only.

---

## 4. Consistency / documentation notes (not defects)

- **N1** — The template `Dependencies/d3dx.ini` does not list the two new keys
  (`auto_refresh_file_to_monitor`, `fuzzy_match_alongside_hash`). Code defaults cover
  this and EDHM ships its own ini, but a commented example in the template would aid
  standalone users and self-document the features.
- **N2** — `Dependencies/d3dx.ini` ships `hide_cursor = 0`, while the `0.1.1`
  CHANGELOG/patch notes recommend `hide_cursor = 1` for EDHM/software-cursor setups.
  This is a soft recommendation, not a required default, but the template and notes
  disagree.
- **N3** — Replacing `if constexpr (sizeof(size_t)==8)` with `#if SIZE_MAX > UINT32_MAX`
  in `ResourceHash.h` is safe: `<stdint.h>` is included in that header, so `SIZE_MAX`
  and `UINT32_MAX` are defined and the 64-bit bit-smear path is retained on x64.
  (Confirmed — flagged only because a missing include here would have silently dropped
  the 64-bit path.)

---

## 5. Areas explicitly confirmed clean

- InputLayout wrapper is never returned to the game on any path; the "game always holds
  a real `ID3D11InputLayout*`" invariant holds across `Create/IASet/IAGet`, ClearState,
  Execute/FinishCommandList, and context destruction (all now call `ClearCurrentInputLayout`).
- `FromLayout` reference accounting matches its contract (`GetPrivateData` AddRef undone,
  returns an AddRef'd side-car the caller releases — done correctly in `IASetInputLayout`
  and `FrameAnalysis::DumpVBs`).
- `ReloadConfig` clears `gReloadConfigPending` up front → the new double-check cannot loop.
- `NextPow2` 64-bit path intact (see N3).

---

## 6. Recommendation

The `develop` branch is **fit for its stated purpose** (an EDHM-compatible `d3d11.dll`
that behaves like classic 3Dmigoto). The compatibility fixes are correct and, in several
cases, also repair pre-existing XXMI defects. None of the findings block the goal.

Suggested follow-ups, in priority order:
1. Guard the `wcsrchr` result and handle UNC paths in the auto_refresh parser (L1).
2. Decide on `EXCEPTION_CONTINUE_EXECUTION` vs `CONTINUE_SEARCH` for ignorable codes and
   document the choice (L2).
3. Add commented `auto_refresh_file_to_monitor` / `fuzzy_match_alongside_hash` examples to
   the template ini and reconcile the `hide_cursor` default with the notes (N1, N2).

The only items with any real-world (if unlikely) crash/robustness exposure are L1 and L2;
everything else is polish. As all changes are Windows-only D3D11 hook code, none of this
was compiled or run in this Linux audit environment — validation relied on source review
against the documented XXMI base and COM/SEH contracts. Live confirmation should follow the
`patches/0001-edhm-xxmi-compat.md` test plan (cold start, station dock, F11/theme reload,
carrier management, quiet `debug=0` logging).
