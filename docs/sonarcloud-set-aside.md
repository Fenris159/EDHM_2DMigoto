# Remaining SonarCloud work after this branch

This branch is based on `main` (the scanned tree). Blockers, High bugs,
sprintf/strcpy overflows on known stack arrays, `NULL`→`nullptr`, DirectXTK
`noexcept` moves, and non-trivial memcpy were fixed. The items below still
need a later pass or are excluded in `sonar-project.properties`.

## Why so many issues exist

SonarCloud reported on the order of **~82k** open issues. The bulk is not
first-party DX11 wrapper logic:

| Bucket | Approx. count | Why it is set aside |
|--------|---------------|---------------------|
| `DirectXTK/Src/Shaders/Compiled/**` scanned as PHP (`php:S1131`) | ~71,700 LOW | Generated shader `.inc` blobs. Not PHP. Now excluded via `sonar-project.properties`. |
| DirectXTK C++ (`cpp:S5018` noexcept moves, field shadowing, etc.) | ~50 BLOCKER + many HIGH/MEDIUM | Microsoft DirectXTK. Forking style into this tree fights upstream and EDHM's "minimum build graph" goal. |
| BinaryDecompiler (`cpp:S4999` memcpy of `Operand`/`Instruction`, `cpp:S3519`) | 13 BLOCKER | Inherited DX9 decompiler. EDHM's product path is D3D11. Changing memcpy of those structs is a real-bug class but belongs in a dedicated decompiler review, not a drive-by DX11 fix. |
| pcre2 / Nektra / crc32c | hundreds | Vendored / prebuilt third-party. |
| HLSLDecompiler `sprintf` (`cpp:S6069`) | ~100 HIGH | Inherited HLSL text emitter. Mechanical `sprintf` → `snprintf` needs per-buffer audit. |
| First-party style HIGH (`NULL` vs `nullptr`, nesting, cognitive complexity, `new`, `void*`) | ~1,800 | 3Dmigoto/XXMI dialect. A mass rewrite would make `xxmi-base` cherry-picks painful and risk hook/COM regressions. |

## Blocker issues set aside

| Rule | Where | Reason |
|------|--------|--------|
| `cpp:S3584` Potential memory leak | `DirectX11/IniHandler.cpp` `ParseNamespacedIniFile` | `std::locale(loc, new codecvt_utf8<...>)` is the standard locale-takes-ownership constructor. False positive. |
| `cpp:S5018` noexcept move | DirectXTK `CommonStates`, `Effects`, `SpriteBatch`, etc. | Microsoft library. Do not restyle. |
| `cpp:S5018` noexcept move | `BinaryDecompiler/include/hlslcc.h` `ShaderVar` | Inherited decompiler types. |
| `cpp:S2387` field shadowing | `DirectXTK/Src/DGSLEffect.cpp` `mDefaultTexture` | Microsoft library. |
| `cpp:S4999` non-trivial memcpy | `BinaryDecompiler/decodeDX9.cpp` `Operand` / `Instruction` | DX9 decoder; EDHM does not ship a D3D9 product. Needs a dedicated decompiler pass. |
| `cpp:S3519` heap index | `BinaryDecompiler/reflect.cpp` `ConstantBuffer` | Same inherited decompiler. |
| `cpp:S4999` non-trivial memcpy | `HLSLDecompiler/DecompileHLSL.cpp` `ResourceBinding_TAG` | Inherited HLSL decompiler. Assignment instead of memcpy needs a type-layout audit. |
| `cpp:S999` goto error cleanup | `DirectX11/CommandList.cpp` `err:` / `out:` in inter-device transfer | This is COM resource cleanup, not a loop. Rewriting to RAII `Release()` guards is valid future work but is a larger refactor than a Sonar rename. |

## High issues set aside

| Rule | Where | Reason |
|------|--------|--------|
| `cpp:S6069` `sprintf` | `HLSLDecompiler/DecompileHLSL.cpp` (many) | Need per-call buffer-size audit. Do not blindly swap. |
| `cpp:S4962` `NULL` → `nullptr` | DirectX11 / D3D_Shaders / util (~900) | Pervasive 3Dmigoto style. Safe but huge and hostile to upstream cherry-picks. |
| `cpp:S5421` global pointer const | Many | COM vtables, hook trampolines, and game-owned pointers are not logically const. |
| `cpp:S134` nesting > 3 | CommandList, IniHandler, Hunting, Hacker* | Shader/ini parsers are inherently branched. Extracting functions is a dedicated refactor. |
| `cpp:S3776` cognitive complexity | Same files | Same. |
| `cpp:S5025` raw `new` | Command list / ini objects | Lifetime is tied to ini reload maps. Unique_ptr conversion is a later pass. |
| `cpp:S5008` `void*` | Hook trampolines, D3D bytecode, Nektra | Required by Win32 / COM / hook APIs. |
| `cpp:S2259` null deref | `DirectXTK/Src/DDSTextureLoader.cpp` | Microsoft loader. |
| `cpp:S2107` uninit fields | `BinaryDecompiler/include/hlslcc.h` | Inherited types. |

## Medium / Low

After compiled-shader exclusion, remaining Medium/Low in first-party code are
almost all style (naming, comments `cpp:S1135`, macros, parameter names).
Do not "fix" those by rewriting the 3Dmigoto ini/command-list language or COM
wrappers just to silence the scanner.

Recommended follow-ups, in order:

1. Confirm `sonar-project.properties` exclusions drop the PHP/compiled-shader
   flood on the next `main` scan.
2. Dedicated review of BinaryDecompiler / HLSLDecompiler memcpy and `sprintf`.
3. Optional, mechanical `NULL` → `nullptr` on a branch that is *not* expecting
   an `xxmi-base` sync.
4. Command-list RAII for the `goto err` / `goto out` COM cleanup paths.

## What this branch did fix

First-party Blockers (identifiers, MSVC goto-over-init, hash_shader jump into
switch, Overlay `&&` increment, IniSection/Override noexcept moves) and
first-party High bugs (CreateShader null out-param, AddFlags argument order,
Assembler type-pun UB, empty `CreateHLSLTextFile`, workflow action SHA pins).
See the commit message / diff for the full list.
