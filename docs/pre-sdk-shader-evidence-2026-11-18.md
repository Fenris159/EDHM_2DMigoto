# Shader evidence before the edo-next SDK

Date: 18 November 2026. Investigation against pre-production checkpoint
`aeadf7a91098d324ff0867c138a374c29e55da2b`; installed files and upstream
packages were read without launching Elite Dangerous. No runtime, preparer,
configuration or shader asset was changed by this investigation.

## Decision

Keep the preparer's conservative signature gate. The one rejected assembly
contains an inherited FSS-overlay metadata inconsistency, plus a separate
temporary-register inconsistency. Neither helper round-trip success nor the
Wine creation result below establishes correct native rendering. We can finish
the evidence classification, native shader-creation validation and preparation
of the explicit 76-asset subset; the later Windows check below now completes
object creation for all 77 inputs. Correcting this optional add-on requires its
own validated asset change; it is not a reason to alter the current DLL or
invent original-shader metadata.

Original game-byte identity remains an external evidence gate. The existing
installation supplies a build string and executable digest, but no matching
original shader dump was recovered. The selected 76-asset subset is not a claim
that all of its features form a self-contained native plugin.

## The FSS overlay is the source of the gap

`ShaderFixes/ea5390a2f20c2368-ps.txt` is 3,331 bytes, SHA-256
`641aade87bbd5f7806a70778a872fb01be7e1cf683a3f09ac454e918ae16a73a`.
The exact bytes also occur in the public [FSS overlay v1.102.2 package][fss-zip]
at EDHM commit `73588af9a1a17cf4aa66aca582afde9d98da60d5`. The package is
1,766,105 bytes, SHA-256
`3e5854c23b6ff4c85c0dc1f682297b0c5099b79b7003898e77bc32b65fb7fb0f`.
This ties the observations to upstream pack content rather than a preparer
rewrite. The locally available EDHM_UI asset copy has the same digest.

| Asset evidence | Consequence |
| --- | --- |
| Line 10's input-signature table contains only TEXCOORD0, register 0, xyzw. | No register-1 signature row is supplied for standalone assembly. |
| Line 33 declares `dcl_input_ps_siv linear noperspective v1.y, position`; its preceding comment says the input was added by the mod. | The replacement adds a system-position read beyond the retained signature comments. An original dump alone would not prove the intended replacement metadata. |
| Line 35 declares `dcl_temps 3`; lines 45, 47 and 48 write/read `r3`. | This falls outside the three declared registers, r0–r2, under Microsoft's definition of `dcl_temps N`. It is a separate source inconsistency. |

The earlier helper-produced DXBC has SHA-256
`c3d81fa91f506a69255b8713e7359750eea6de103198d0ef11d2b853c3691443`.
Its ISGN lacks register 1 as well. All 77 inputs passed the helper's assembly,
Flugan round-trip comparison and Microsoft disassembly. The later stricter
preparer rejects this asset before publishing a complete manifest; its explicit
76-asset subset passes. [Preparation receipts][receipts]

Microsoft documents a system-input declaration as a register expecting a
system value, and pixel-stage `SV_Position` as screen-space pixel position.
Its signature examples include `SV_Position` among ordinary shader inputs.
These facts support examining both declaration and signature data; they do not
prove that this particular container must fail every runtime. Some system
inputs use special-purpose operands rather than ordinary signature rows, as
the helper's source also notes. The preparer's supported declaration policy
must therefore remain distinct from a universal Direct3D validity rule.
[System-input declaration][input-sv], [position semantics][semantics],
[signatures][signatures], [helper system values][system-values]

The temporary-register finding follows directly from the declared count and
used register. Microsoft's example explains that count 10 declares r0–r9;
count 3 therefore does not declare r3. This is a static inconsistency, not an
executed Windows validation failure. [Temporary-register declaration][temps]

## What the runtime and helper actually do

The runtime copies incoming original bytecode before assembling a replacement.
With `assemble_signature_comments=0`, the assembler reuses original-container
sections. With the inspected installation's value of 1, it manufactures the
signature sections from assembly comments instead. The manufactured-container
route does not silently acquire the missing row from the original game shader.
[Runtime assembly call][runtime-asm], [helper selection][helper-selection]

The helper's round-trip compares its own assembly/disassembly result; this is
a different gate from D3D11 shader creation and pipeline linkage. Microsoft's
`CreatePixelShader` supports bytecode validation even with a null output pointer,
returning `S_FALSE` on successful validation. Actual creation and release of a
shader is also an available off-game gate. Neither proves the intended FSS
image, its preceding shader outputs or its resource bindings.
[CreatePixelShader][create-ps], [stage linkage][linkage]

## Bounded execution evidence

A private x64 probe compiled with the already pinned LLVM-MinGW toolchain read
the 77 unchanged helper outputs and called `CreatePixelShader` or
`CreateVertexShader` according to stage. Host Wine 10 used built-in D3D11/DXGI;
`D3D11CreateDevice` was requested with `D3D_DRIVER_TYPE_WARP`, returned `S_OK`
and feature level 11_0. All 77 shader creation calls returned `S_OK`, including
the FSS asset. No draw, swap chain, game launch or FPS measurement occurred.

Wine reported adapter `ATI Radeon HD 5600 Series`, vendor 4098/device 26840.
That is the API's reported identity on this route, not proof of that physical
GPU or Microsoft WARP. The result demonstrates acceptance on this Wine route;
it neither clears the static inconsistencies nor predicts native Windows or
future-host behavior. Native Windows WARP was a separate gate at this checkpoint;
the later collection below supplies that object-creation evidence.

Private research receipts, probe source and logs are under
`.quality/pre-sdk-completion/shader-research/`; no binaries or mod assets are
redistributed by this report. The probe source SHA-256 is
`bbf32e94f42362890e239a512c8e7d1ff3f8d44ca25cb026a2409657b277fd8a`.
Verify the receipt for the complete executable/input digests before reusing
this experimental result.

## Later native Windows object-creation evidence

The [native collection at `2b03e6be`](https://github.com/Fenris159/EDHM_2DMigoto/actions/runs/44341473187)
reconstructed every replacement from the pinned public sources described below.
System D3D11 WARP accepted all 77 actual shader objects in both x64 and Win32
processes, with zero rejected objects. The authored PS/VS and malformed-input
checks also passed. The source/bytecode files remained unchanged.

The [x64](benchmarks/pre-sdk-completion-2026-11-18/pack-native-x64.json) and
[Win32](benchmarks/pre-sdk-completion-2026-11-18/pack-native-Win32.json) receipts
record per-shader source/prepared digests and HRESULTs. Prepared bytes match
across architectures; the 76 strict outputs match the earlier Wine preparation.
The FSS asset was assembled and probed separately, while the strict preparer
continued to reject its missing declared input-signature metadata. It was not
added to a successful strict manifest.

Thus neither source inconsistency is a demonstrated shader-object creation
failure on these tested routes. The successful call does not establish intended
linkage, register contents, resource bindings, pixels or future-host acceptance.
Keep those distinctions when proposing an asset change. The collector removes
temporary assets, bytecode and tools and retains only observation metadata.

## Existing original/build evidence

The read-only installation inspection found:

- `ShaderCache/`: zero files. `ShaderFixes/`: 77 text files and no original
  shader binary. A bounded search of existing Downloads/game-project files
  found a second copy of the replacement text, not an original container.
- `VersionInfo.txt`: `2026.10.23.334925`, 122 bytes, SHA-256
  `4da4862a1270862385a8dffff2fb8392e4693d1457c108de736c994d1d5c70c1`.
- `EliteDangerous64.exe`: 103,481,344 bytes, SHA-256
  `ed5a8f8cd10f9a80d3bda5badd7ba1d4fcfc15996c8c7e018852cb170e264b63`.
  A bounded scan found one raw contiguous DXBC container; its zero-seeded
  full-buffer key did not match `ea5390a2f20c2368`. Packed assets and
  runtime-generated bytecode were outside that scan.
- Existing `d3d11_log.txt`: SHA-256
  `275ad5a0f4500ca1a5b83103a66c98d469ef34efc7072b310e20a21b5f2d3a56`.
  It begins with DLL v0.1.4 on 15 November, records `shader_hash=3dmigoto` and
  `assemble_signature_comments=1`, and shows the target key while parsing
  configuration. Those lines do not contain original bytes or demonstrate that
  this replacement was created/rendered successfully. They are not logs of the
  new preliminary branch.

The executable and version digests identify this installation when inspected.
They do not retroactively identify the game build behind the replacement's
2023 comment or any separately acquired shader dump. A hash-like filename is a
claim to verify, not original identity. [Identity contract][identity]

## Retrieval and acceptance recipe without new game monitoring

1. Inspect an **existing** original shader export, archived capture or maintainer
   fixture. Preserve its archive/file digest, export provenance, stage,
   capture-time game build and associated executable digest. A screenshot or
   configuration key cannot substitute for raw bytes.
2. Distinguish original exports in `ShaderCache` from compiled replacements in
   `ShaderFixes`. The runtime's `ExportOrigBinary` writes original incoming
   buffers before replacement when binary export was enabled; its separate
   replacement path caches assembled output in `ShaderFixes`. Duplicate export
   names can have numeric suffixes. [Original exporter][export-original],
   [export order][export-order], [replacement cache][replacement-cache]
3. On a read-only copy, bound-check the DXBC container and stage; record its
   size and SHA-256. Independently calculate the configured legacy key. For
   this installation's `3dmigoto` mode, start at zero and apply
   `h = ((h * 0x100000001b3) XOR octet) mod 2^64` to the complete exact buffer.
   Require the calculated key and stage to match the intended rule. Preserve
   collisions/multiple candidates rather than choosing by filename.
4. Keep original identity, replacement-source identity and prepared-byte identity
   as distinct records. Obtain original/replacement signature and intended
   preceding-stage evidence before proposing a repair for the FSS input.
   A validated author-supplied correction can then be tested on an immutable
   copy; do not synthesize a comment row from the filename or assume changing
   the temporary count alone repairs rendering.
5. If no existing original bytes/provenance can be supplied, leave those fields
   unknown. A new game capture would be separately authorized work outside this
   preparation session. The future host may supply original bytes through its
   introspection interface, but its draft is not current evidence.

This completes the available evidence investigation. Remaining external inputs
are original-byte provenance, intended FSS replacement metadata/output evidence
and asset-distribution permission. Native validation of prepared inputs can
advance independently of those inputs and independently of the future SDK.

## Reconstructing the entire input set in ephemeral CI

All 77 installed assembly replacements have exact public matches in two
archives at the same EDHM commit, `73588af9a1a17cf4aa66aca582afde9d98da60d5`:

| Public archive | Exact installed matches | Archive bytes | SHA-256 |
| --- | ---: | ---: | --- |
| [EDHM_Odyssey_v22.04.zip][base-zip] | 73 | 3,663,081 | `4c0c016086650920f94b6702ba7737f89b43d10eda5351dc83743335ba270b57` |
| [FSS_Overlay_Mod_ED4.3.1.0_v1.102.2.zip][fss-zip] | 4 | 1,766,105 | `3e5854c23b6ff4c85c0dc1f682297b0c5099b79b7003898e77bc32b65fb7fb0f` |

The four FSS members are `04fc7f00bade5b4a-ps.txt`,
`1d5a317f0e831e9d-ps.txt`, `d6c6b9abdf104b6d-ps.txt` and the rejected
`ea5390a2f20c2368-ps.txt`. No installed replacement was absent or mismatched.
The private machine-readable reconstruction recipe is
`.quality/pre-sdk-completion/public-shader-inputs.json`; it records archive URLs
and exact member names, sizes, SHA-256 digests and stages for all 77 inputs.

An ephemeral Windows job can verify each pinned archive and selected member,
assemble immutable copies with the pinned helper/compiler, and exercise the
native checker without uploading the downloaded archives or prepared shaders.
Retain only hashes, status and logs. The strict preparer must still reject the
known metadata-gap asset; testing its unchanged helper output separately must
not turn that rejection into a complete-manifest claim. Public availability
does not grant permission to redistribute an altered pack.

The exact helper/compiler pair is also publicly reconstructible from the
[original 3Dmigoto 1.4.9 release archive][assembler-release]:
`3Dmigoto_v1.4.9.7z`, 4,695,932 bytes, SHA-256
`d5d05e78f0f6df7cce8ef10c669c088db6b8dbe1428ce783d8c3c93ac842dce1`.
GitHub's release asset ID is 358684109; the inspected asset metadata gives that
same archive digest. Verify the archive before extracting only these members:

| Archive member | Bytes | SHA-256 |
| --- | ---: | --- |
| `cmd_Decompiler/cmd_Decompiler.exe` | 599,040 | `625a252161cb8ba7927c72287fa436badae0d19b4079d7dd2b3207b1af2a05ac` |
| `x32/d3dcompiler_47.dll` | 4,147,544 | `0ae6e52072857c3acfde53b98f56c3c6d9bd0e7b09f247ae7f40bfd451593c4c` |

Both tool members were verified as PE i386. Their hashes match the prior
preparation experiment. A release URL/tag is mutable, so archive and member
hashes are required content pins. The archive contains a file named
`Version-1.4.10.txt` despite its published tag of 1.4.9; version-name inference
must not replace byte verification. The private reconstruction record is
`.quality/pre-sdk-completion/public-assembler-inputs.json`. The public, committed
recipes are [shader inputs](../RuntimeTests/fixtures/public-shader-inputs.json)
and [assembler inputs](../RuntimeTests/fixtures/public-assembler-inputs.json).
The [preparer guide](offline-dxbc-preparer.md#independent-native-shader-object-check)
documents the optional collector lane; no downloaded payload is bundled.

[fss-zip]: https://github.com/psychicEgg/EDHM/blob/73588af9a1a17cf4aa66aca582afde9d98da60d5/Odyssey/3rdPartyMods/FSS-Overlay/FSS_Overlay_Mod_ED4.3.1.0_v1.102.2.zip
[base-zip]: https://github.com/psychicEgg/EDHM/blob/73588af9a1a17cf4aa66aca582afde9d98da60d5/Odyssey/EDHM_Odyssey_v22.04.zip
[assembler-release]: https://github.com/bo3b/3Dmigoto/releases/tag/1.4.9
[receipts]: benchmarks/edo-pre-production-follow-up-2026-11-16/README.md#offline-preparation
[input-sv]: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dcl-input-sv
[semantics]: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics
[signatures]: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-signatures
[system-values]: https://github.com/Fenris159/EDHM_2DMigoto/blob/aeadf7a91098d324ff0867c138a374c29e55da2b/D3D_Shaders/SignatureParser.cpp#L89
[temps]: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dcl-temps
[runtime-asm]: https://github.com/Fenris159/EDHM_2DMigoto/blob/aeadf7a91098d324ff0867c138a374c29e55da2b/DirectX11/HackerDevice.cpp#L977
[helper-selection]: https://github.com/Fenris159/EDHM_2DMigoto/blob/aeadf7a91098d324ff0867c138a374c29e55da2b/D3D_Shaders/SignatureParser.cpp#L841
[create-ps]: https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createpixelshader
[linkage]: https://learn.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-d3d9-to-d3d10-considerations#shader-signatures-and-linkage
[identity]: edo-next-readiness-2026-11-16.md#dependency-map-and-shader-identity
[export-original]: https://github.com/Fenris159/EDHM_2DMigoto/blob/aeadf7a91098d324ff0867c138a374c29e55da2b/DirectX11/HackerDevice.cpp#L538
[export-order]: https://github.com/Fenris159/EDHM_2DMigoto/blob/aeadf7a91098d324ff0867c138a374c29e55da2b/DirectX11/HackerDevice.cpp#L1284
[replacement-cache]: https://github.com/Fenris159/EDHM_2DMigoto/blob/aeadf7a91098d324ff0867c138a374c29e55da2b/DirectX11/HackerDevice.cpp#L992
