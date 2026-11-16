# Passive EDHM inventory

`scripts/inventory-edhm.py` records configuration and replacement evidence for
maintaining the current pack and preparing a future edo-next port. It reads
files without loading the DLL, running commands, compiling shaders or launching
the game. Python's standard library is sufficient.

```bash
python scripts/inventory-edhm.py /path/to/product/d3dx.ini --output inventory.json
python -m unittest discover -s RuntimeTests -p test_readiness_inventory.py -v
```

The output must be a new file. An existing file, including an input INI, is
never overwritten. Keep installation inventories outside the repository or in
an ignored `.quality/` directory; they describe a particular user's pack.
`--max-files` defaults to 10,000 and bounds recorded files and traversed
directories. Exceeding the limit fails the command.

The JSON records:

- Exact byte lengths and SHA-256 digests, without rewriting source encoding.
- Explicit and recursive include candidates, root filename exclusion patterns,
  duplicate/cyclic includes, and literal-zero preamble exclusions.
- Candidate section counts, INI row references, direct texture-query occurrences,
  effectful `checktextureoverride` commands and Region expressions.
- Shader/texture matching fields with their original file and line.
- First-observed settings affecting hashing, directories, signature assembly and
  startup. Conflicting observations receive a diagnostic; this is not a runtime
  setting-selection result.
- Replacement/cache filenames, stage claims, declared assembly shader models and
  presence of signature-comment headers. Header presence does not prove that
  signatures are complete or assembly succeeds.

Original shader identity and game build remain **unverified**. A replacement's
text, filename or cached binary cannot establish which original container the
game submitted. Preserve the original hash contracts documented in the
[main plan](edo-next-readiness-2026-11-16.md#dependency-map-and-shader-identity).

This is a candidate inventory, not another INI interpreter. It does not execute
conditions, merge namespaced sections, choose active overrides or reproduce
runtime include scheduling. A nonliteral preamble condition is marked unknown;
its descendants remain candidates. Exclusions use case-insensitive filename
glob matching rather than the runtime's PCRE2 conversion. Absolute Windows
paths need a prepared local layout; unavailable paths and undecodable files
receive diagnostics. Directory/file cycles terminate by resolved identity.

Inspect diagnostics and activation fields before interpreting counts. The
inspected local installation produced nine INI records, 77 assembly replacements
and 503 TextureOverride sections. That is one pack's static evidence; it proves
no performance gain, game-build identity or D3D12 compatibility.
