# Advanced Shader Hunting

Advanced shader hunting adds an optional second level to the existing 3DMigoto
hunting workflow. Level 1 continues to select complete shaders. Level 2 holds
the selected pixel or vertex shader and cycles through the distinct draw
contexts that use it.

The feature is intended for shader authors. It is disabled by default and adds
no context tracking to normal release play.

## Enable It

Enable both hunting and the optional context level in `d3dx.ini`:

```ini
[Hunting]
hunting = 1
advanced_hunting = 1
marking_mode = skip
```

The complete settings and default key bindings are documented in the shipped
`Dependencies/d3dx.ini` template.

## Workflow

1. Use the existing Level 1 keys to select a pixel or vertex shader.
2. Press `Ctrl+Numpad 0` to enter Level 2.
3. Allow the scene to render while draw contexts are collected.
4. Use `Ctrl+Numpad 1` and `Ctrl+Numpad 2` to cycle the contexts.
5. Watch the overlay and the game image before marking a context.
6. Press `Ctrl+Numpad 3` to mark the selected context.
7. Press `Ctrl+Numpad +` to return to shader-level hunting.

When `marking_mode=skip`, entering Level 2 restores the rest of the Level 1
shader and hides only the selected context. This makes the currently targeted
draw observable in the game before it is marked. `pink` and `original` use the
same selected-context boundary.

## Controls

| Key | Action |
|-----|--------|
| `Ctrl+Numpad 0` | Enter or leave Level 2 using the current PS or VS selection |
| `Ctrl+Numpad 1` | Select the previous context |
| `Ctrl+Numpad 2` | Select the next context |
| `Ctrl+Numpad 3` | Mark the selected context and export its parent shader |
| `Ctrl+Numpad 4` | Select the previous grouping scope |
| `Ctrl+Numpad 5` | Select the next grouping scope |
| `Ctrl+Numpad 6` | Lock or resume live context collection |
| `Ctrl+Numpad +` | Clear Level 2 and return to Level 1 |

All controls are configurable in the `[Hunting]` section.

## Grouping Scopes

| Scope | Context identity |
|-------|------------------|
| `resource` | Bound index/indirect buffers, vertex buffers, pixel shader resources, render targets, and depth target |
| `auto` | Resource identity plus draw type, primitive topology, and draw counts |
| `draw` | Auto identity plus first vertex, first index, first instance, and indirect argument offsets |

`auto` is the recommended starting point. Use `resource` to reduce a noisy
context list or `draw` when repeated draws share resources and counts but use
different offsets. Changing scope clears the collected list so it can be
rebuilt with the new identity.

## Overlay And Marking

The Level 2 overlay shows the parent shader, selected context position, grouping
scope, collection state, draw signature, current-frame match count, and a stable
context fingerprint. Set `context_overlay_verbose=1` to also show the resource
hashes used by the selected context.

Marking a context still exports the parent shader through the existing
`marking_actions`. When clipboard marking is enabled, it also copies a
best-effort `TextureOverride` example containing a stable observed resource hash
and applicable draw matching fields. Review that example before adding it to an
INI; the runtime does not modify active configuration files automatically.

## Safety And Limits

- The registry is bounded by `context_max_entries` and expires stale entries
  using `context_lifetime_frames`.
- Collection is CPU-side state tracking. It does not enable frame analysis,
  perform GPU readback, or replay draw calls.
- Level 2 separates complete DX11 draw calls. If several icons are submitted in
  one batched draw, they will still disappear together.
- Exact per-icon separation inside one batch would require game-specific data
  analysis or experimental draw replay. That behavior is deliberately outside
  this first implementation.
- Contexts based on dynamic resources can change between frames. Lock collection
  with `Ctrl+Numpad 6` after the useful set has appeared.
