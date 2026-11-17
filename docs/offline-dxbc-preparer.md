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
