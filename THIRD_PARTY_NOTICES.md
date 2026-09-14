# Third-Party Notices

EDHM_2DMigoto incorporates third-party software components. This file records
their versions, provenance, licenses, and local packaging state, as required
by those licenses and by the project's release-readiness audit (findings
F-10/F-11). It must be included in every binary release archive together with
the license files referenced below.

## Project lineage

| Component | Upstream | License |
|---|---|---|
| 3Dmigoto (base codebase) | https://github.com/bo3b/3Dmigoto | GPLv3 |
| XXMI-Libs-Package (intermediate fork) | https://github.com/SpectrumQT/XXMI-Libs-Package | GPLv3 |
| EDHM_2DMigoto (this project) | https://github.com/Fenris159/EDHM_2DMigoto | GPLv3 (`LICENSE.GPL.txt`, `COPYING.txt`, `AUTHORS.txt`) |

Source repository and commit for any given binary release are recorded on the
GitHub release page for that tag; checksums are published alongside the
binaries (`*.sha256` files).

## Bundled dependency inventory

| Dependency | Version | Provenance | License | Local modifications | Notes |
|---|---|---|---|---|---|
| PCRE2 (headers + prebuilt static libs `pcre2/pcre2-8-*.lib`) | 10.30 (2017-08-14) | https://github.com/PCRE2Project/pcre2/releases/tag/pcre2-10.30 | BSD 3-clause with binary exemption (`pcre2/LICENCE`) | None known; prebuilt `.lib` binaries of unknown compiler/flags (inherited from 3Dmigoto tree) | Used for `ShaderRegex` pattern matching with JIT enabled (`pcre2_jit_compile`, `DirectX11/ShaderRegex.cpp`). Upgrade tracked as release-engineering debt — replace prebuilt libs with source builds in one reviewable change with regex regression tests. |
| DirectXTK (source snapshot `DirectXTK/`) | 2017-12-13 snapshot | https://github.com/microsoft/DirectXTK | MIT (`DirectXTK/LICENSE`) | Warning suppressions scoped to this project only (`DirectXTK_Desktop_2017.vcxproj`) | Only the subset compiled into `d3d11.dll` (e.g. SpriteFont/SpriteBatch for the overlay) is shipped; do not bulk-update without validation. |
| crc32c-hw (`crc32c-hw-1.0.5/`) | 1.0.5 | Bundled source (zlib-style license in `crc32c-hw-1.0.5/src/crc32c.cpp` header) | zlib-style (see file header) | `__cpuid` include fix for VS2017 noted in source | Runtime CPUID dispatch with software fallback. |
| Deviare-InProcess (`Nektra/`) | 3Dmigoto-bundled snapshot | Nektra (https://github.com/nektra) | GPLv3 | Inherited from 3Dmigoto | Used for LoadLibraryExW / DXGI factory hooks. |
| BinaryDecompiler / HLSLDecompiler / D3D_Shaders assembler | 3Dmigoto lineage | https://github.com/bo3b/3Dmigoto | GPLv3 | EDHM maintenance changes | Flugan-derived assembler + DXBC handling. |

## Microsoft components (not vendored here)

`d3dcompiler_47.dll` packaged with releases is the Microsoft redistributable
from the Windows SDK (or EDHM's own package), redistributed under the Windows
SDK license terms. It is not built from this repository.

## Obligation summary

- **GPLv3** (project + 3Dmigoto lineage): binary releases must be accompanied
  by license text and a source offer — satisfied by bundling
  `LICENSE.GPL.txt`, `COPYING.txt`, `AUTHORS.txt` and the repository URL/tag.
- **PCRE2 BSD**: binary redistributions must reproduce the copyright,
  conditions and disclaimer — satisfied by bundling `pcre2/LICENCE` (as
  `PCRE2_LICENCE.txt` in release archives).
- **DirectXTK MIT**: the notice must be included in copies or substantial
  portions — satisfied by bundling `DirectXTK/LICENSE` (as
  `DirectXTK_LICENSE.txt` in release archives).
