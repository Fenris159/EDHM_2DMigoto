# Runtime development guidance

Read [the root guidance](../AGENTS.md) and the relevant
[command contract](../docs/ini-command-reference.md) first. These rules also
apply when changing a shared helper on behalf of the runtime.

## Compatibility boundaries

The DLL executes inside the game's process. Preserve D3D/COM signatures, calling
conventions, wrapper behavior and resource ownership. Track borrowed versus
owned resource/view references through every success and failure path. Use the
existing RAII and ownership patterns; local modernization must preserve the ABI
and lifetime contract.

Keep the ordinary and strict build configurations working. Shared
`Directory.Build.targets` supplies required language settings and compatibility
definitions independently of warning mode. Preserve the focused project graph;
consult the upstream port before adding a dependency.

## Configuration and references

- Resolve reference bind/misc requirements after all relevant sections have been
  parsed. Preserve section-order independence, namespaces, multi-hop references
  and cycle termination. Clear deferred state before reload discards its objects.
- Keep scalar and local variable lookup precedence intact when extending pool
  syntax. Preserve the supported namespaced, Unicode and hyphenated identifiers.
- Honor the documented persistent-settings lifecycle. Failed saves preserve
  pending changes and the last complete file; replacement remains atomic. Check
  reload, removed persistence, unknown values and explicit wipe together when
  changing `IniHandler.cpp` or `PersistentSettingsState.h`.

## Pool metadata and copy destinations

- Metadata queries must leave live pool elements, mappings, expiration state and
  templates untouched, including nested variable/resource expressions and a
  skipped `unless_null` copy. Detached previews execute ordinary lookup rules for
  lazy slots, FIFO, spatial reuse and the once-per-frame expiration gate.
- Preserve learned bind flags and keep STATIC operands pinned to their parsed
  object across proxy changes. Aliases of one physical root slot share the same
  provisional GPU resource; an accepted copy adopts it once while a skipped
  copy releases it. Preserve CPU-cache ownership, range and creation metadata.
- Resolve a dynamic copy destination once per command. Retain both its physical
  resource and evaluated logical key. Intervening same-pool source reads may
  rekey a slot; final assignment restores the destination association without
  evaluating the expression again or selecting another physical slot.

Read `PoolMetadataPreview`, `ResourceCopyTarget`, `CustomResourcePool` and
`CopyResourceToResource` together before changing this path. Existing metadata,
alias, expiration and destination-key cases protect these interactions.

## Buffers and views

- `store` failures preserve the destination variable's previous value. Validate
  source kind, full typed/structured elements, bounds and owned-view release.
- `Region(offset,size)` uses a byte count. Distinguish backing-buffer bounds,
  copy alignment and view extents; preserve smaller constant-buffer copies and
  the stricter alignment rules for constant-buffer references.
- Reuse a view only when resource identity, effective interface/kind and active
  descriptor agree, including range, format, stride and raw mode. Cover warm
  caches, changing `this` targets and runtime-learned flags. A readable retained
  SRV still needs a writable view for `clear`; failed view creation must preserve
  the existing pipeline binding.

## Profiling and evidence

Preserve collection-window registration and release stale command pointers
before reload. A zero-frame report must retain newly collected reload work.
Keep instrumentation conditional on the intended profiling mode and measure its
overhead separately from profiling-disabled game behavior.

For regression work, use [the test process](../CONTRIBUTING.md#testing) and the
production-path cases in [RuntimeTests](../RuntimeTests/README.md). Use actual
MSVC/WARP or CI results for Windows claims and keep game validation distinct.
