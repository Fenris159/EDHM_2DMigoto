# Offline DXBC preparation

`scripts/prepare-edhm-dxbc.py` prepares selected SM5 pixel/vertex assembly from
an existing [passive inventory](readiness-inventory.md). It does not load the
product DLL, install files, rewrite shaders or implement an edo-next plugin.
Python's standard library is sufficient for the wrapper.

The external tools are the existing 3Dmigoto command-line helper and an
architecture-matched Microsoft `d3dcompiler_47.dll`. Supply and pin both SHA-256
values. Tools are not bundled. The known 1.4.9 x86 pair used for the preparation
experiment has these digests:

| File | SHA-256 |
| --- | --- |
| `cmd_Decompiler.exe` | `625a252161cb8ba7927c72287fa436badae0d19b4079d7dd2b3207b1af2a05ac` |
| x86 `d3dcompiler_47.dll` | `0ae6e52072857c3acfde53b98f56c3c6d9bd0e7b09f247ae7f40bfd451593c4c` |

For a Linux preparation run, use an explicit isolated Wine prefix:

```bash
python scripts/prepare-edhm-dxbc.py prepare \
  --root /path/to/product --inventory /path/to/inventory.json \
  --output /path/to/new-prepared-directory \
  --assembler /path/to/cmd_Decompiler.exe \
  --compiler /path/to/matching/d3dcompiler_47.dll \
  --assembler-sha256 625a252161cb8ba7927c72287fa436badae0d19b4079d7dd2b3207b1af2a05ac \
  --compiler-sha256 0ae6e52072857c3acfde53b98f56c3c6d9bd0e7b09f247ae7f40bfd451593c4c \
  --wine /usr/bin/wine --wine-prefix /path/to/isolated-prefix
```

On Windows, use native paths and omit the Wine options. Use PowerShell's
backtick for line continuation. Repeat `--asset ShaderFixes/<key>-ps.txt` to
select a subset explicitly; otherwise all inventoried replacements are selected.
The output must be a new directory. Existing directories, files and input paths
are rejected. Inventory paths containing parent traversal or absolute paths
need a prepared local layout. Existing installation symlinks can supply inputs;
their bytes are copied, not modified.

The wrapper checks every inventoried file's size and digest, then copies the
pinned tools and selected assembly into temporary staging directories. Assembly
and validation use separate directories because the stock helper can overwrite
same-basename assembly while disassembling. It requires:

- One `ps_5_0` or `vs_5_0` model matching the conventional filename stage.
- Input/output signature comments and numeric input/output declarations.
- Bounded, non-overlapping DXBC chunks, matching model/code extent, and supported
  ISGN/OSGN tables covering all declared registers and components.
- Successful assembly, an explicit successful Flugan round trip and successful
  Microsoft disassembly confirming the model.
- Unchanged staged inputs and unchanged inventory/tool digests at completion.

The declaration-coverage rule includes system-value declarations. It is a
conservative preparation policy; missing comment metadata does not by itself
prove the installed D3D11 mod fails. Unsupported declarations/signature versions
are rejected rather than inferred. Microsoft default disassembly is not
reassembled: its decimal literals can lose precision.

A successful selected batch contains `shaders/*.dxbc`, diagnostic `logs/` and
`manifest.json`. No helper binaries or assembly inputs are left in the output.
The manifest records content identities, signature counts and a recipe key.
Game-original container/build identity stays explicitly unverified. Register
assignments remain unchanged, including `t120`; there is no host-ABI transform.
An error leaves `.incomplete` and, when writable, `failure.json`; no successful
manifest is accepted while the marker exists. Choose a new output directory for
a retry. No previous successful output is replaced.

Use the same root, inventory, tool paths and pins with action `check` to verify
an existing result. It checks current source/tool/output bytes and the recorded
recipe without executing the helper, so Wine is unnecessary for verification.
Execution-route metadata is separate from the content recipe. Input/tool
changes invalidate the result even when filenames and timestamps are retained.
This is checksum/provenance verification, not a publisher signature.

```bash
python -m unittest discover -s RuntimeTests -p test_dxbc_preparer.py -v
```

The tests substitute only the external process boundary. They use a frozen,
authored minimal assembly/DXBC fixture and check complete output, immutable
inputs, stale recipes, architecture mismatch, helper/signature/container
failures and refusal to overwrite output. Deliberate faults must be detected.
Native shader-object creation, rendered output and future-host compatibility
remain separate gates in the [main plan](edo-next-readiness-2026-11-16.md).
Prepared files are not a current-runtime cache or a distributable plugin pack;
asset permissions and host integration must be settled before distribution.

## Independent native shader-object check

`RuntimeTests/PreparedShaderTests.vcxproj` builds a separate off-game checker:

```powershell
msbuild RuntimeTests/PreparedShaderTests.vcxproj -target:Build `
  -property:Configuration=Release -property:Platform=x64 `
  -property:EdhmStrictBuild=true -nodeReuse:false -verbosity:minimal -restore:false
& .quality/prepared-shaders/x64/PreparedShaderTests.exe --self-test
& .quality/prepared-shaders/x64/PreparedShaderTests.exe ps C:/private/prepared/panel.dxbc vs C:/private/prepared/vertex.dxbc
```

Use `Platform=Win32` for the other architecture. The checker loads system D3D11
and requests WARP at feature level 11_0, with no hardware fallback. It creates
and releases actual shader objects, requires every input to remain unchanged,
and returns 1 if creation fails. Each file is bounded to 16 MiB. Self-checks use
authored PS/VS sources and malformed bytecode; CI also checks the frozen 212-byte
authored PS through the file interface and requires malformed input to return 1.

This result is independent of preparation metadata policy. It neither changes
the preparer's manifest nor establishes stage linkage, bound resources,
rendered output or a future host's accepted profiles. A Wine run reports the
requested driver; it does not establish Microsoft WARP or physical-GPU coverage.
The [shader evidence report](pre-sdk-shader-evidence-2026-11-18.md) explains the
remaining FSS asset gap and distinguishes original identity from prepared bytes.

The optional manual runtime input `shader_pack_validation=true` collects the
public pack's native acceptance on both Windows architectures. Its runner is
`RuntimeTests/run_prepared_shader_pack.py`, with two source archives and the
helper/compiler pinned in `RuntimeTests/fixtures/public-*-inputs.json`.
Only exact named members are read; no archive tree is installed. Assembly,
bytecode and external tools live in a temporary directory that is removed.
Artifacts contain JSON observations/hashes only, never the downloaded assets,
bytecode, compiler or helper disassembly logs.

The collector's `status=collected` means its observations are complete. Check
`accepted`, `rejected`, `all_assets_accepted` and each HRESULT before claiming
native acceptance. A shader rejection remains explicit even when collection
succeeds. Structural exclusions remain independent; the FSS asset's unchanged
assembly is probed separately without adding it to the strict manifest.
The source recipe reproduces this installation's replacements, not game-original
identity or asset-distribution rights.
