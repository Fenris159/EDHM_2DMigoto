# INI command reference

This is the command-list and configuration language implemented by
`EDHM_2DMigoto`’s `d3d11.dll` (3Dmigoto / XXMI-derived). It documents what the
parser in `DirectX11/IniHandler.cpp` and `DirectX11/CommandList.cpp` actually
accepts as of the XXMI **v0.9.5–v0.9.8** port.

Items marked **(new)** arrived after EDHM `v0.1.3-alpha.5` (2026-07-17).

`d3dx.ini` is the root file. EDHM then includes `EDHM-ini/*.ini`. Any included
file can use the same language.

---

## 1. How to think about it

The DLL intercepts Elite’s D3D11 calls. An **ini command list** is a short
program that runs at a hook point (a shader draw, Present, a key press, …).

Typical EDHM HUD work looks like:

1. **Match** a shader or texture (`[ShaderOverride]`, `[TextureOverride]`).
2. **Set numbers** the replacement shader can read (`x = …`, `$var = …`).
3. **Optionally bind or copy** resources (`ps-t120 = ResourceFoo`).
4. **Optionally skip or replace** the original draw (`handling = skip`,
   `run = CustomShaderBar`).

Two value families exist:

| Family | Lives in | Visible to HLSL | Namespaced | Example |
|--------|----------|-----------------|------------|---------|
| **IniParams** | GPU constant buffer (`IniParams`) | Yes (`IniParams[n].xyzw`) | No | `x = 1`, `y2 = rt_width` |
| **Named variables** | CPU floats | Only if you copy them into IniParams | Yes (`$\file\name`) | `global $hud = 1` |

IniParams are what replacement shaders usually read. Named variables are better
for state, keys, and math, then you assign them into `x`/`y`/`z`/`w` when the
shader needs them.

Every command list has two phases:

| Prefix | When it runs |
|--------|----------------|
| *(none)* or `pre` | **Before** the original D3D call (draw, Present, …) |
| `post` | **After** the original call |

```ini
[ShaderOverrideExample]
hash = 0123456789abcdef
x = 1                 ; before the draw
ps-t120 = ResourceFoo ; bind a texture the replacement shader samples
post x = 0            ; restore after the draw
```

---

## 2. Files, includes, namespaces

### `[Include]`

| Key | Purpose |
|-----|---------|
| `include = path.ini` | Load one extra ini (DLL-relative). |
| `include_recursive = dir` | Load every `.ini` under a directory. |
| `exclude_recursive = glob` | Skip names while recursing (`DISABLED*`, `desktop.ini`). |

EDHM’s root file loads `EDHM-ini/…` this way.

Included files get a **namespace** from their path. Named variables and
`[Resource]` / `[CommandList]` / `[CustomShader]` names from that file are
prefixed so two mods can both declare `$mode` without clashing:

```ini
; from EDHM-ini\ThemeSettings.ini you can refer to:
$\EDHM-ini\ThemeSettings.ini\mode
```

Inside the same file you usually just write `$mode`.

`F10` reloads config (`reload_config` in `[Hunting]`). Not every setting is
live-reloadable; if something looks stale, restart the game.

---

## 3. Global template sections

These live in `d3dx.ini` (or an include). They are **not** command lists except
where noted.

### `[Loader]`

Used only by an external 3Dmigoto loader. EDHM’s normal install drops
`d3d11.dll` beside `EliteDangerous64.exe` and ignores this.

| Key | Meaning |
|-----|---------|
| `target` | Process name the loader looks for (`EliteDangerous64.exe`). |
| `module` | DLL to inject (`d3d11.dll`). |
| `launch`, `delay`, `require_admin` | Loader-only. |

### `[Hunting]`

Developer overlay and shader hunting. Leave `hunting = 0` for players.

| Key | Meaning |
|-----|---------|
| `hunting` | `0` off (fast), `1` on, `2` off but toggleable. |
| `marking_mode` | `skip` / `original` / `pink` / `mono` on the selected shader. |
| `marking_actions` | What F-keys dump: `hlsl asm regex clipboard` … |
| `next_*` / `previous_*` / `mark_*` | Cycle pixel/vertex/compute/… shaders and buffers. |
| `reload_fixes` | Reload `ShaderFixes` (default `F10`). |
| `reload_config` | Reload ini (often also `F10`). |
| `wipe_user_config` | Delete `d3dx_user.ini` and reload. |
| `show_original` | Hold to disable the fix (`F9`). |
| `analyse_frame` | Dump a frame of D3D state (`F8`). |
| `analyse_options` | Global dump flags (`dump_rt dump_tex dump_cb …`). |
| `monitor_performance` | Overlay command-list CPU cost. |
| `verbose_overlay` | Show hashes on the overlay. |
| `repeat_rate` | Auto-repeat for held hunting keys. |

Hunting keys use the same binding syntax as `[Key]` (see below).

### `[Logging]`, `[System]`, `[Device]`, `[Rendering]`

Operational knobs, not per-draw commands. The important EDHM ones:

| Section | Key | Use |
|---------|-----|-----|
| `[Logging]` | `enabled` | Open `d3d11_log.txt` without API spam. |
| `[Logging]` | `calls` | Legacy verbose API log (slow). |
| `[System]` | `skip_early_includes_load` | EDHM default `0` so TextureOverrides exist before the first frame. |
| `[System]` | `auto_refresh_file_to_monitor` | DLL-relative path; Present polls it and reloads config (EDHM_UI theme). |
| `[System]` | `wine_compat` | Wine/Proton profile. |
| `[Device]` | `hide_cursor` | Hide OS cursor for themed cursors. |
| `[Rendering]` | `stereo_params` | EDHM default `-1` (no StereoParams). HUD uses `ini_params`. |
| `[Rendering]` | `fuzzy_match_alongside_hash` | Off by default: hash TextureOverrides win before fuzzy match. |
| `[Rendering]` | `track_region_hashes` | Enable region hashing (needed for `->HashRegion`). |

### `[Constants]` — command list at load / F10

Runs once at startup and after config reload. Declare globals and set initial
IniParams here.

```ini
[Constants]
global $hud_enabled = 1
global persist $user_scale = 1.0
global locked $pi = 3.14159          ; (new) cannot be assigned later
x = 0
y = $user_scale
```

**(new)** Values here can be **expressions**, evaluated once at parse time:

```ini
[Constants]
global $half = 1920 / 2
x = saturate($user_scale)
```

### `[Present]` — command list every frame

Runs at `IDXGISwapChain::Present` (after the frame is submitted). Good for
cursor HUD, once-per-frame bookkeeping, **not** for per-draw shader work.

```ini
[Present]
if $hud_enabled
    run = CustomShaderCursor
endif
```

### `[Key…]` — input bindings

Section name must start with `Key`. Assignments in the section are a command
list **plus** preset-style IniParam/`$var` sets.

| Key | Meaning |
|-----|---------|
| `key` | Binding (see syntax below). Multiple `key=` lines share state. |
| `back` | Cycle backwards (`type = cycle`). |
| `type` | `hold` (while pressed), `toggle`, `cycle` (default for lists). |
| `smart` | Cycle resyncs if the current values no longer match. |
| `wrap` | Cycle wraps around (`true`/`false`). |
| `delay` / `release_delay` | ms delay (`hold` only). |
| `transition` / `release_transition` | ms to lerp values. |
| `transition_type` | `linear` or `cosine`. |
| `condition` | Expression; binding ignored when false. |
| `$var` / `x` / `y` … | Values to apply. Cycle: `x = 0, 0.5, 1`. |
| `run = CommandListFoo` | Run a command list (post phase on release for hold/toggle). |

**Key syntax**

- Letters/digits: `z`, `F10`.
- Virtual keys: `VK_F9`, `RBUTTON`, `NO_MODIFIERS VK_NUMPAD1`.
- Combos: `ctrl alt F10`, `Shift Q`, `NO_ALT F1`.
- Xbox: `XB_A`, `XB_LEFT_TRIGGER`, `XB2_LEFT_SHOULDER` (controller 2).

### `[Preset…]`

Named bundles of IniParam/`$var` values. Activate from a command list with
`preset = PresetFoo` or `exclude_preset = PresetFoo`.

**(new)** Variable names in presets are parsed with `CommandArgumentReader`
(namespaces and whitespace work the same as elsewhere).

---

## 4. Matching sections (when a command list runs)

### `[ShaderOverride…]`

Runs when the game sets a vertex/pixel/compute/… shader whose **hash** matches.

| Key | Meaning |
|-----|---------|
| `hash` | 16 hex digits (shader hash). Required. |
| `allow_duplicate_hash` | `true` / `overrule` — several sections may share a hash. |
| `filter_index` | Number other lists can read via `ps` / `vs` / … operands. |
| `model` | Override compiled shader model when replacing from `ShaderFixes`. |
| `depth_filter` | Deprecated; prefer `x = oD` + a shader branch. |
| `disable_scissor` | Compatibility alias; injects a built-in custom shader. |
| *(any command)* | Command list (see §5). |

```ini
[ShaderOverrideHudIcon]
hash = 3c69e169edc8cd5f
x = 1
ps-t120 = ResourceThemeIcon
```

Put the replacement in `ShaderFixes\3c69e169edc8cd5f-ps_replace.txt` (or `.bin`).

### `[TextureOverride…]`

Runs when a matching resource is used (bound, copied, etc.), depending on
`checktextureoverride` and automatic matching.

**Identity match**

| Key | Meaning |
|-----|---------|
| `hash` | Texture/buffer hash. |

**Fuzzy description match** (cannot mix with `hash=`)

`match_type`, `match_usage`, `match_bind_flags`, `match_cpu_access_flags`,
`match_misc_flags`, `match_format`, `match_byte_width`, `match_stride`,
`match_mips`, `match_width`, `match_height`, `match_depth`, `match_array`,
`match_msaa`, `match_msaa_quality`.

Numeric matches accept a small expression:
`[operator] value | field_name [ * field_name ] [ * k ] [ / d]`
e.g. `match_width = >= width * 2`.

**Draw-context extra filters** (can combine with hash or fuzzy)

`match_first_vertex`, `match_first_index`, `match_first_instance`,
`match_vertex_count`, `match_index_count`, `match_instance_count`.

**Overrides / policy**

| Key | Meaning |
|-----|---------|
| `filter_index` | Value `ps-tN` style operands return when this resource is bound. Default if omitted: `1.0`. Unmatched slot: `0.0` or `-0.0` if empty. |
| `match_priority` | Order when several sections match. Also silences duplicate-hash warnings. |
| `width` / `height` / `width_multiply` / `height_multiply` | Change size when the game creates the resource. |
| `override_vertex_count` + `override_byte_stride` | Grow a vertex buffer (vertex-limit raise). |
| `uav_byte_stride` | UAV element count when resizing. |
| `deny_cpu_read` | Block `Map` readback. |
| `expand_region_copy` | Expand region copies. |
| `iteration` | Legacy iteration list. |
| *(any command)* | Command list. |

```ini
[TextureOverrideCompass]
hash = 8a1b2c3d
filter_index = 2
```

In a ShaderOverride: `y = ps-t0` then the replacement shader can `if (IniParams[0].y == 2)`.

### `[CustomShader…]` / `[BuiltInCustomShader…]`

A full shader program you `run`. Files are HLSL (`vs`/`ps`/`cs`/…) relative to
the ini or the DLL directory. `null` unbinds that stage.

| Key | Meaning |
|-----|---------|
| `vs` `hs` `ds` `gs` `ps` `cs` | Source filename or `null`. |
| `flags` | `D3DCompile` flags (space-separated names). |
| `max_executions_per_frame` | Safety cap. |
| Blend / depth / rasterizer / sampler / topology | Override pipeline state for this shader (see `CustomShaderIniKeys` in `IniHandler.cpp`: `blend`, `depth_enable`, `fill`, `cull`, `scissor_enable`, `topology`, `sampler`, `*_state_merge`, …). |
| *(any command)* | Usually ends with `draw = …` or `dispatch = …`. |

```ini
[CustomShaderFullscreen]
vs = quad.hlsl
ps = tint.hlsl
handling = skip
draw = from_caller
```

### `[CommandList…]` / `[BuiltInCommandList…]`

A reusable list you `run = CommandListFoo`. Same commands as other lists.
`post run = CommandListFoo` runs only its post phase.

### `[Resource…]`

Declares a custom resource (texture, buffer, or empty placeholder).

| Key | Meaning |
|-----|---------|
| `filename` | Load from disk (DDS/PNG/JPG via WIC, or raw for buffers). |
| `type` | `Buffer`, `StructuredBuffer`, `ByteAddressBuffer`, `Texture2D`, `Texture3D`, `TextureCube`, `RW*` variants. |
| `format` | DXGI format name (`R8G8B8A8_UNORM`, …). |
| `width` `height` `depth` `mips` `array` `msaa` `msaa_quality` `byte_width` `stride` | Create-from-scratch size. |
| `width_multiply` `height_multiply` | Scale vs a copied source. |
| `bind_flags` | Replace automatic bind flags (`shader_resource render_target` …). |
| `misc_flags` | Misc resource flags. |
| `data` | Inline initial buffer contents. |
| `max_copies_per_frame` | Rate limit. |

```ini
[ResourceThemeIcon]
filename = EDHM-ini/icons/target.png
```

Then bind it: `ps-t120 = ResourceThemeIcon`.

### `[Pool…]` **(new)**

A bank of resources and/or variables, indexed by a number (draw id, hash,
spatial cell, …).

| Key | Default | Meaning |
|-----|---------|---------|
| `pool_size` | 1 | Slot count. |
| `pool_index_type` | `ring` | `ring` (direct index), `static`, `fifo` (id → slot table), `spatial`. |
| `pool_lazy_initialization` | true | Create GPU objects on first use. |
| `pool_element_type_switch_reset` | true | Reset slot when switching resource ↔ variable. |
| `pool_allocate_slot_on_missing` | false | FIFO/spatial: allocate on **read** of an unknown id. |
| `pool_expiration_timeout_frames` | none | Recycle slots unused this long. |
| `pool_expiration_reset_elements` | true | Reset expired slots. |
| `pool_expiration_refresh_on_read` | false | Reading a slot counts as use. |
| `pool_spatial_radius` | 1 | Spatial neighbour reuse (Chebyshev cells). |
| `pool_persist_variables` | false | Persist variables (`ring` only). |
| `pool_variable_default_value` | 0 | Default `$PoolName[i]`. |
| *(same keys as `[Resource]`)* | | Template for each slot (`filename`, `type`, …). |

**When to use:** many similar objects (icons, world markers) that would
otherwise need hundreds of `[Resource]` sections. Typical EDHM HUD does **not**
need pools; they matter if you start instancing.

```ini
[PoolMarkers]
pool_size = 64
pool_index_type = fifo
type = Texture2D
format = R8G8B8A8_UNORM
width = 64
height = 64

[ShaderOverrideMark]
hash = ...
ps-t121 = PoolMarkers[$id]
$PoolMarkers[$id] = $selected
```

### `[ShaderRegex…]`

Regex-replace shader assembly by pattern. Subsections compile a group; if any
part fails the whole group is disabled. Advanced; see hunting
`marking_actions = regex`. Command lists can attach to a group.

---

## 5. Command-list commands

These keys are valid in `[ShaderOverride]`, `[TextureOverride]`,
`[CustomShader]`, `[CommandList]`, `[Constants]`, `[Present]`, and `[Key]`
(as `run =`).

Prefix with `pre` or `post` if you need a phase (default is pre).

### Control flow

| Command | What it does | Typical use |
|---------|--------------|-------------|
| `if <expr>` … `elif` / `else if` … `else` … `endif` | Branch. Locals declared inside are scoped to the block. | Gate HUD on `$enabled`, hunting, empty RT. |
| `handling = skip` | Skip the **original** draw/dispatch (pre only). | Replace a draw with your CustomShader. |
| `handling = abort` | Stop this list **and** callers (pre and post). | Bail if the wrong depth target is bound. |
| `run = CustomShaderName` | Run a `[CustomShader]`. | Fullscreen pass, extra HUD draw. |
| `run = CommandListName` | Run a `[CommandList]`. | Share logic between overrides. |
| `preset = PresetName` | Apply a preset. | Switch a bundle of depths/scales. |
| `exclude_preset = PresetName` | Undo / exclude a preset. | |
| `reset_per_frame_limits = …` | Reset `max_executions_per_frame` / copy caps. | Allow another run this frame. |

```ini
if $hud_enabled && hunting == 0
    run = CustomShaderReticle
endif
```

### Draws (usually inside `[CustomShader]`)

| Command | Arguments | Use |
|---------|-----------|-----|
| `draw = from_caller` | — | Replay the game’s current draw (after `handling = skip`). |
| `draw = auto` | — | `Draw` using an inferred vertex count. |
| `draw = <count> <start>` | expressions | `Draw(count, start)`. |
| `drawauto` | — | `DrawAuto`. |
| `drawindexed = auto` / `drawindexed = <idx> <start> <base>` | | Indexed draw. |
| `drawindexedinstanced` | 5 args or `auto` | |
| `drawinstanced` | 4 args | |
| `dispatch = <x> <y> <z>` | | Compute. |
| `drawindexedinstancedindirect` / `drawinstancedindirect` / `dispatchindirect` | buffer target | Indirect args from a resource. |

### Resources and copies

The general form is:

```text
[pre|post] <destination> = [options…] <source>
```

Destination and source are **resource targets** (§6).

| Option | Meaning |
|--------|---------|
| *(none)* | Reference if safe, otherwise copy. |
| `copy` | Always copy into a new (or compatible) resource. |
| `ref` / `reference` | Bind the same object (must have compatible bind flags). |
| `copy_desc` / `copy_description` | Copy the description only (allocate empty compatible resource). |
| `unless_null` | Do nothing if the source is unbound. |
| `set_viewport` | Set viewport from the destination size. |
| `no_view_cache` | Don’t reuse cached views. |
| `raw` | Raw buffer view. |
| `mono` | Force mono (stereo path; unused for EDHM). |

```ini
; Bind a custom texture to a spare slot the replacement shader samples
ps-t120 = ResourceThemeIcon

; Snapshot the current render target
ResourceHudCopy = copy o0

; Unbind
ps-t120 = null

; Copy only if something is bound
ResourceMaybe = copy unless_null ps-t0
```

Incompatible bind flags: add `copy` explicitly
(`vs-cb0 = copy ResourceRWBufferCB`).

`this` is the resource that triggered a TextureOverride / analysis list.

### IniParams and variables

```ini
x = 1.5                          ; IniParams[0].x
y2 = rt_width / rt_height        ; IniParams[2].y
w = ps-t0                        ; filter_index of whatever is in ps-t0

global $scale = 1
global persist $remember = 0
global locked $max_icons = 32    ; (new) assignment later is an error
local $tmp = vs-cb0->size        ; (new) block-scoped

$scale = saturate($scale + 0.1)
x = $scale                       ; push into the shader
```

IniParam names: `x y z w` then `x1`…`w7` (index 0–7). The DLL grows the
IniParams buffer to fit the highest index you use.

### `store` **(new rewrite)**

Read GPU memory into a named variable (uses the shared staging-buffer pool).

```ini
store = $value ResourceFoo
store = $value vs-cb0 16          ; optional byte offset (expression)
```

**Use for:** driving HUD from a constant buffer the game already filled
(health, zoom, …) without writing a custom shader.

This **stalls the GPU** if overused. Prefer once per frame in `[Present]`, not
on a hot ShaderOverride.

### `clear`

Clear a render target, depth, or UAV.

```ini
clear = o0
clear = oD 1.0                    ; depth
```

### Frame analysis

| Command | Meaning |
|---------|---------|
| `analyse_options = dump_tex dds persist` | Change dump flags from this list. `persist` keeps them. |
| `dump = dump_tex dds ps-t0` | Dump one target with per-resource options. |

### Special

| Command | Meaning |
|---------|---------|
| `special = upscaling_switch_bb` | Flip between fake/real backbuffer after upscaling. |
| `special = draw_3dmigoto_overlay` | Force-draw the hunting overlay. |
| `checktextureoverride = o0` | Run TextureOverride lists for that slot/resource. |
| `commandlist… = …` **(new)** | Copy/proxy a command list (callback / ShaderRegex). |

### Input-layout overrides **(new)**

Change a vertex-element format or offset for the **current** input layout,
then bind a newly created **real** D3D layout (EDHM never gives the game a
wrapper pointer).

```ini
; Widen BLENDINDICES on vb0
vb0->ElementFormat(BLENDINDICES, 0) = R16G16B16A16_UINT

; Move a semantic
vb0->ElementOffset(BLENDWEIGHTS, 0) = 8
```

**Use for:** mesh/skinning mods. Almost never needed for 2D EDHM HUD.

---

## 6. Resource targets

Left-hand and right-hand sides of copies, `clear`, `dump`, `store`, and
texture-filter operands.

### Pipeline slots

`s` is the shader letter: `v` vertex, `h` hull, `d` domain, `g` geometry,
`p` pixel, `c` compute.

| Token | Meaning |
|-------|---------|
| `vs-t0` … `ps-t127` / `cs-tN` | Shader resource (texture) slot. |
| `vs-cb0` … `ps-cb13` / `cs-cbN` | Constant buffer slot. |
| `ps-u0` … / `cs-uN` | UAV (pixel or compute only). |
| `vb0` … `vb31` | Vertex buffer slot. |
| `ib` | Index buffer. |
| `so0` … | Stream-output buffer. |
| `o0` … `o7` | Render target. |
| `oD` | Depth/stencil. |
| `bb` | Current backbuffer (after/before upscale — see below). |
| `r_bb` | Real swap-chain buffer. |
| `f_bb` | Fake / upscaling buffer. |
| `null` | Unbind / empty (source only). |
| `this` | Resource that triggered this list. |
| `iniParams` | The IniParams buffer itself. |
| `cursor_mask` / `cursor_color` | Software cursor bitmaps. |
| `ResourceName` | A `[Resource]` section. |
| `PoolName` / `PoolName[$id]` | A `[Pool]` slot **(new)**. |
| `$var` | A named variable (as a numeric source). |

Shader type in `ps-t0` must be one letter of `vhdgpc`.

### Prefixes **(new / extended)**

| Prefix | Meaning |
|--------|---------|
| `@ResourceFoo` | Identity / pointer-ish id (for comparisons). |
| `#PoolFoo` | Pool size (number of slots). |
| `$PoolFoo[i]` | The **variable** stored in that pool slot (not the texture). |

### Member getters **(new)**

Written `target->Member` or `target->Member(args)`. Case-insensitive.

| Member | Args | Returns / does |
|--------|------|----------------|
| `->Size` | — | Byte size (resource) or pool slot count (`#Pool` equivalent). |
| `->Stride` | — | Structured stride. |
| `->SourceStride` | — | Stride of the source when copying. |
| `->Offset` | — | Bind offset. |
| `->Index` | — | Pool slot index for this id. |
| `->LastFrame` | — | Frame number when that pool slot was last written. |
| `->Region(offset, size)` | byte offset, size | View/copy only that CB span (`cs-cb0 = ref vs-cb0->Region($o, $s)`). |
| `->HashRegion(offset, size)` | byte offset, size | Hash of that byte span (needs `track_region_hashes`). |
| `->SpatialHash(xOff, yOff, zOff, cell)` | 3 offsets + cell size | Spatial cell key for pool indexing. |
| `->ElementFormat(SEMANTIC, index)` | name, semantic index | **Assign** a new DXGI format (layout override). |
| `->ElementOffset(SEMANTIC, index)` | name, index | **Assign** a new aligned byte offset. |

```ini
; Scale HUD from the RT size
x = o0->size

; Copy 16 bytes starting at $off from the vertex-shader CB
cs-cb0 = ref vs-cb0->Region($off, 16)

; Hash a constant-buffer window (hunting / matching)
$h = vs-cb0->HashRegion(0, 64)
```

Use `#PoolName` for pool size (slot count) and `->Stride` for a resource's
structured stride.

---

## 7. Expressions

Anywhere a number is accepted (`x =`, `if`, `draw =`, member args, **(new)**
`[Constants]`), you can write an expression.

### Literals

| Form | Example |
|------|---------|
| Decimal | `1`, `0.25`, `-0.0` |
| Hex | `0x1a` |
| **(new)** Binary | `0b1010` |
| Named var | `$scale`, `$\file.ini\scale` |
| IniParam | not as a bare name — assign *to* `x`/`y` instead |

`-0.0` is special for texture filters: **nothing bound** (distinct from `0.0`).
Compare with `===` if you need that bit pattern.

### Operands (built-in names)

Use these on the **right-hand side** (`x = rt_width`, `if hunting`).

| Name | Meaning |
|------|---------|
| `rt_width` `rt_height` | Current render target. |
| `res_width` `res_height` | Resolution (backbuffer). |
| `window_width` `window_height` | Client area. |
| `effective_dpi` | UI scale helper (not raw DPI). |
| `vertex_count` `index_count` `instance_count` | Current draw. |
| `first_vertex` `first_index` `first_instance` | Draw args. |
| `thread_group_count_x/y/z` | Current dispatch. |
| `indirect_offset` `draw_type` | Indirect / kind of call. |
| `cursor_showing` | OS cursor visible? |
| `cursor_screen_x/y` | Screen pixels. |
| `cursor_window_x/y` | Client pixels. |
| `cursor_x/y` | Client coords mapped to `[0,1]`. |
| `cursor_hotspot_x/y` | Cursor hotspot. |
| `time` | Seconds since launch (`GetTickCount64`). |
| `frame_number` **(new)** | Present count. |
| `draw_number` **(new)** | Draws so far this frame. |
| `dispatch_number` **(new)** | Dispatches so far this frame. |
| `scissor_left/top/right/bottom` | Optional index: `scissor_left[0]`. |
| `hunting` `frame_analysis` | 1 if that mode is on. |
| `sli` `stereo_active` `stereo_available` | Stereo/SLI (usually 0 for EDHM). |
| `ps` `vs` `cs` … | `filter_index` of the **current** shader of that type. |
| `ps-t0` `vs-cb1` … | `filter_index` of the resource in that slot (or 0 / -0.0). |

### Operators

Precedence high → low (same idea as C / HLSL):

| Group | Tokens |
|-------|--------|
| Unary | `!` `~` `+` `-` |
| Functions | see below (prefix, one argument) |
| Power | `**` (right-associative) |
| Mul | `*` `/` `//` (floor div) `%` |
| Add | `+` `-` |
| Shift | `<<` `>>` |
| Compare | `<` `<=` `>` `>=` |
| Equality | `==` `!=` `===` (bitwise identical) `!==` |
| Bitwise | `&` `^` `\|` |
| Logic | `&&` `\|\|` |

`ps-t0` is one token (resource slot), not `ps` minus `t0`. Write `ps - t0` if
you truly want subtraction (there is no `t0` operand).

### Functions **(new set completed in 0.9.x)**

Prefix form: `sin($x)`, `saturate(rt_width / 1920)`.

| Function | Result |
|----------|--------|
| `sin` `cos` `tan` `asin` `acos` `atan` | Radians. |
| `abs` `sign` | Sign is -1 / 0 / +1. |
| `ceil` `floor` `trunc` `round` `frac` | |
| `sqrt` `rsqrt` | |
| `exp` `exp2` `log` `log2` | |
| `saturate` | Clamp to `[0,1]`. |
| `countbits` | Popcount of the value as `uint`. |
| `random` | Random float in `[0, arg)`. |

---

## 8. Worked EDHM-oriented examples

### Show a theme icon on a known HUD shader

```ini
[ResourceReticle]
filename = EDHM-ini/icons/reticle.png

[ShaderOverrideReticle]
hash = aabbccddeeff0011
ps-t120 = ResourceReticle
x = 1
```

Replacement pixel shader samples `t120` when `IniParams[0].x == 1`.

### Drive a shader from a named, persistent setting

```ini
[Constants]
global persist $opacity = 0.8

[KeyOpacityUp]
key = OEM_PLUS
$opacity = saturate($opacity + 0.05)

[ShaderOverrideHud]
hash = ...
x = $opacity
```

`$opacity` is saved to `d3dx_user.ini` on F10 / exit.

### Skip the original draw and draw your own quad

```ini
[CustomShaderDim]
vs = fullscreen.hlsl
ps = dim.hlsl
handling = skip
draw = from_caller

[ShaderOverrideScene]
hash = ...
run = CustomShaderDim
```

### Run extra work only when a texture is the compass

```ini
[TextureOverrideCompass]
hash = 11223344
filter_index = 9

[ShaderOverrideAnyHud]
hash = ...
if ps-t0 == 9
    x = 1
endif
```

### Read 4 bytes from a game constant buffer once per frame **(new)**

```ini
[Constants]
global $zoom = 0

[Present]
store = $zoom vs-cb1 12
x = $zoom
```

Offset `12` is bytes into that bound VS constant buffer.

### Pulse using frame number **(new)**

```ini
[ShaderOverridePulse]
hash = ...
x = 0.5 + 0.5 * sin(frame_number * 0.08)
```

---

## 9. EDHM-specific behaviour (vs stock XXMI)

These are **not** extra commands; they change defaults or safety:

- `[Include]` loads `EDHM-ini/…`, not a generic `Mods` folder.
- `skip_early_includes_load` defaults off so TextureOverrides exist on frame 0.
- Hash TextureOverrides win unless `fuzzy_match_alongside_hash = 1`.
- `stereo_params` default `-1` (HUD uses IniParams).
- `auto_refresh_file_to_monitor` reloads when EDHM_UI writes a file.
- Input layouts stay **real D3D pointers**. Layout override commands still
  work; they never bind a C++ wrapper (that AVs Elite).
- Present time uses 64-bit ticks (`time`, `frame_number` stay monotonic).

---

## 10. Quick command index

| Want to… | Use |
|----------|-----|
| Change a replacement shader’s inputs | `x=` / `y=` / `ps-tN=` on `[ShaderOverride]` |
| Remember a user setting | `global persist $name` + `[Key]` |
| Make a true constant | `global locked $name` **(new)** |
| Draw extra HUD | `[CustomShader]` + `run=` + `draw=` |
| Hide the original mesh | `handling = skip` |
| Stop a nested list | `handling = abort` |
| Branch | `if` / `endif` |
| Reuse logic | `[CommandList]` + `run=` |
| Load a PNG/DDS | `[Resource]` `filename=` |
| Instance many similar resources | `[Pool]` **(new)** |
| Read GPU data into CPU state | `store =` **(new)** |
| Slice a constant buffer | `->Region(off, size)` **(new)** |
| Hash part of a buffer | `->HashRegion` **(new)** (enable `track_region_hashes`) |
| Animate without a key | `time` / `frame_number` / `draw_number` **(new)** |
| Remap a vertex semantic | `vb0->ElementFormat` **(new)** |
| Dump one resource | `dump = dump_tex ps-t0` |
| Reload while playing | `F10` (`reload_config` / `reload_fixes`) |

Parser source of truth: `DirectX11/IniHandler.cpp`, `DirectX11/CommandList.cpp`.
Template comments: `Dependencies/d3dx.ini`.
