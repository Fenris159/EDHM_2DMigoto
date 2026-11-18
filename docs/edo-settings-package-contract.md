# Proposed edo-next settings and package contract

Prepared 18 November 2026 for the SDK-independent part of
[the readiness plan](edo-next-readiness-2026-11-16.md), ranks 3 and 5.
Status: **local proposal, ready for host/editor review**. It does not establish
agreement with EDUP, EDHM or EDHM_UI, define a published host ABI, or authorize
an installation or redistribution. No runtime, editor or release feed changes
are made by this document.

## Decisions that can be made now

Keep three kinds of data separate: versioned program/default files, editable
user themes, and disposable derived/cache files. A package update owns the
first kind only. Preserve the current D3D11 DLL feed and use a separate identity
for a future native plugin. Import existing theme data through an explicit,
completed-file handoff, then publish complete parameter snapshots. Do not
translate the whole INI interpreter into another backend.

The current runtime manifest already contains version, source tag, channel,
architecture, size, SHA-256 and tag-pinned URLs for `d3d11_edhm.dll`.
[Manifest generator][runtime-manifest], [published-cache snapshot][runtime-feed].
The host draft's packaging/update sections propose a separate plugin folder,
manifest, settings directory and ZIP feed. These remain proposed host contracts.
[Draft §§7–9][draft], [investigated draft revision][draft-status].

| Kind | Owner | Update/import rule |
| --- | --- | --- |
| Plugin binary, authored defaults, permitted shader replacements, notices | Package publisher | Immutable within a version; replace as one verified package |
| Selected theme, profile INIs, editor metadata, user preferences | User/editor | Never overwritten by a package update; changes create a new settings generation |
| Runtime hotkey/persistent state | Plugin | Separate from editor-owned theme files; save only explicitly supported fields |
| Prepared DXBC, bake/cache files, generated row previews | Their producing tool/host | Rebuild from pinned inputs; never treat them as the user's original theme or the game's original shader |
| Migration evidence and source backups | Local importer/user | Read-only after creation; no automatic cleanup by a package update |

## Names and local layout

Use `edhm.hud` as a **working plugin ID** pending agreement. It is not a domain
claim, host reservation or permission to distribute EDHM. A future entry DLL
could be `edhm_edo.dll`; it must not reuse the proxy names `d3d11.dll` or
`d3d11_edhm.dll`. The existing runtime keeps `edup-runtime.json` and its current
download assets. A native plugin would use its own version/channel and a
separate `<id>.edo-update.json` feed. No such feed or plugin exists yet.
[Existing release contract][release-guide], [draft §§7–8][draft].

Before an SDK exists, stage local evidence outside the game:

```text
<new-preparation-directory>/
  preparation.json                 our evidence record; not a host manifest
  evidence/
    inventory.json                 exact input evidence
    preparer-manifest.json         selected output identities
    migration.json                 source hashes, supported fields, exclusions
  private-assets/                  optional local copies; never implied distributable
    prepared-shaders/
  host-mapping/
    requirements.md                unresolved SDK fields and services
```

Create a new directory and refuse existing destinations, including an input
file. Keep the directory clearly marked as preparation: no `edo-plugin.json`,
entry DLL, enabled plugin folder, guessed ABI version or success claim for a
native host. This extends the existing [inventory](readiness-inventory.md) and
[preparer](offline-dxbc-preparer.md) roles without changing them.

Once supported by a real host, resolve package/settings/cache paths through the
host interface. The draft's example settings location is
`%LOCALAPPDATA%/edo-next/plugins/<id>/`; it is a proposal, not a portable path to
hard-code. On Proton it must resolve within the intended prefix. An importer
takes an explicit destination; it must not search every prefix or infer the
active game installation. [Draft §§5–7][draft].

The proposed settings subtree is host-independent within that resolved root:

```text
settings/
  active.json                      editor/importer-owned committed generation
  generations/000001/
    ThemeSettings.json             unmodified metadata copy when supplied
    profiles/                      unmodified selected profile INIs
    snapshot.json                  bounded supported values plus provenance
  runtime-state.json               plugin-owned fields only
  migration/<import-id>/           read-only source evidence and diagnostics
```

Generation names and filenames above are our proposed settings format, not
names promised by the SDK. An initial importer should create a separate copy;
it should not repoint EDHM_UI's current game instance or change existing
junctions/symlinks. Switching the game's renderer/profile remains explicit.

## EDHM_UI handoff and literal-value mapping

At EDHM_UI commit `3f5807ac66b7ced8ad0a1741e412f3891f9f2bb1`, theme elements
identify their destination using `File`, `Section` and `Key`. Single-key values
are written directly; multi-key colours are decoded and gamma-corrected before
writing individual INI values. XML-profile entries use their own key/value
list. Therefore a packed JSON colour integer is not a parameter-row float.
[Editor conversion][editor-conversion].

Preserve the editor's existing format and conversion. The proposed handoff is:

1. EDHM_UI writes its supported profile files using its existing conversion.
   A future integration publishes a completed-batch descriptor only after every
   write succeeds. Its current `SaveThemeINIs` already waits for the selected
   file writes and reports failures. [Editor batch save][editor-save].
2. The importer reads that declared batch into a new generation, records exact
   sizes/hashes, validates supported values, then rechecks every source. A
   missing or changed source rejects the batch. Rechecking alone cannot prove
   that an editor was between writes: without a producer completion signal,
   require an explicit completed export/apply operation while edits are paused.
3. Keep JSON as metadata/round-trip material. Read the already converted INI
   literals for the first numeric importer. A future direct JSON importer needs
   independent conversion fixtures and explicit ownership/licensing review.
4. Publish the generation by replacing `active.json` only after validation.
   The future plugin reads and validates one complete generation on its plugin
   thread, then requests one complete host parameter update at a legal point.

The following mappings illustrate the inspected local pack; they are data
addresses, not proof that a future host supports the associated HUD feature.
The local source digests are recorded below; no theme files are redistributed.

| Existing input | Observed value / meaning | Proposed snapshot result |
| --- | --- | --- |
| `XML-Profile.ini`, `[Constants]`, `x150` | `0.65`, one matrix component | Original logical row 150, x component; parse once to float32 |
| `Startup-Profile.ini`, `[Constants]`, `z105` | `100`, a shield mode value | Original logical row 105, z component; retain the numeric value |
| JSON `File=Advanced`, `Section=Constants`, `Key=x232\|y232\|z232\|w232` | Packed editor colour | Follow the editor-generated four INI literals; do not split or reinterpret the integer as row data |
| Namespaced `global persist $name` | Named saved variable | Inventory separately; import only through an explicit supported name/type mapping |
| `rt_width`, resource queries, conditional/post assignments, scene flags | Runtime-dependent values/effects | Preserve source and report unsupported; do not flatten into a static theme |
| `[Key]` cycles/transitions or custom command lists | Ordered behavior | Inventory behavior; no implicit execution or automatic translation |

Preserve logical row/component keys, including holes; do not compact the
numbering. A hand-authored row 1 `(1,2,3,4)` remains row 1, with other unspecified
components initialized to zero in the bounded snapshot. Store canonical 32-bit
value bits when exact identity is required. Reject non-finite/out-of-range
inputs and ambiguous duplicate/conditional assignments. A literal record alone
does not establish the final runtime value after includes, presets or post
commands. The importer must name its supported subset and exclusions.
[Existing assignment implementation][assignments], [rank 5 constraints][value-plan].

The host's constant-header rows, reserved register and rebasing rules must not
be baked into imported settings. SDK integration maps our logical rows to the
host block. The existing preparer retains t120 until that transform is actually
available and validated. [Draft §§5.1–5.2][draft], [preparer scope](offline-dxbc-preparer.md).

## Change detection, saved state and failure behavior

EDHM_UI currently schedules a delayed, mtime-only retry after a theme save; the
runtime polls the signal file at most every two seconds and schedules a full
configuration reload. A file touch is therefore not a new theme generation.
Use the committed generation/content identity for the future importer, rather
than translating each touch into another application of values.
[Editor retry][editor-retry], [current runtime polling][runtime-watch].

Only one editor/importer writer may commit a settings generation at a time;
refuse a second writer for that settings root. Prepare an immutable generation
in the same destination filesystem, validate it, flush completed writes, then
replace the active descriptor atomically using the destination platform's
supported replacement operation. A failure leaves the previous descriptor and
complete generation usable. An abandoned generation is not active. A symlink,
reparse point or path escape must not redirect a write into the original theme
or package tree.

The plugin owns `runtime-state.json` separately, so a hotkey save cannot replace
the editor's source files. Snapshot pending state before a save; changes made
during I/O remain pending. Failed persistence retains both pending changes and
the last complete file. Reload must not race an in-flight save. These outcomes
follow the current runtime's existing save/reload contract; implementation reuse
needs a concrete second consumer rather than a new generic storage adapter.
[Current atomic save][runtime-save], [persistence ownership][runtime-persistence].

Keep unknown fields/source data in migration evidence without applying them.
Do not silently adopt the D3D11 unknown-variable cleanup policy for a new JSON
format: the existing policy has a one-load grace and an explicit indefinite
retention option, tied to declared persistent variables. Any native equivalent
needs an intentional versioned policy. [Current policy][runtime-persistence].

Notification settings are independent. A future `reload_confirmation` user
preference may suppress only this plugin's success notification; validation
errors remain logged and changed values must still apply. This is not an EDUP
F8-menu override. The D3D11 runtime continues to use root `[Logging]`
`show_reload_message=0` for its reload confirmation and `show_warnings=0` for
warning visibility. [Current notification contract](ini-command-reference.md#logging-system-device-rendering).

## Integrity record and future release/update mapping

The local preparation record should use `prep_schema_version: 1` and
`artifact_kind: "edhm-edo-preparation"`. This schema is ours; never put the
host's `schema_version` or ABI claim on a preparation folder.

| Field | Required content / rule |
| --- | --- |
| `source` | Runtime source commit and tool versions/digests; an input's digest is independent of filename/mtime |
| `inputs` | Relative path, exact byte size and lowercase SHA-256 for each checked source; no absolute path needed for portable identity |
| `prepared_outputs` | Relative path, byte size, SHA-256 and the producing recipe key; selection/exclusion reason for omitted assets |
| `settings_subset` | Supported literal fields, row extent and explicit unsupported/uncertain records; no claim of interpreter equivalence |
| `game_original_identity` | `unverified` until original containers and a proven hash/build mapping are supplied |
| `permissions` | Per-asset provenance/permission status and evidence reference; `missing` prevents a distribution claim |
| `host_mapping` | `pending_sdk`; list required capabilities without fabricated numeric versions/layouts |
| `distribution_ready` | `false` while native entry/checker, permissions or original identity gates remain |

For example, this incomplete local receipt makes no host/load claim. Omitted
inputs and outputs still need their own records before a tool can claim a
complete preparation bundle:

```json
{
  "prep_schema_version": 1,
  "artifact_kind": "edhm-edo-preparation",
  "source": { "runtime_commit": "aeadf7a91098d324ff0867c138a374c29e55da2b" },
  "inputs": [{
    "path": "EDHM-ini/XML-Profile.ini",
    "size_bytes": 1108,
    "sha256": "122a9fadf4d778e6bb9e2d1b909a2a8c25d5da62985d8fddfc42a2507c9855ab"
  }],
  "prepared_outputs": [],
  "settings_subset": { "status": "not_imported", "row_extent": null },
  "game_original_identity": { "status": "unverified" },
  "permissions": [{ "path": "EDHM-ini/XML-Profile.ini", "status": "missing" }],
  "host_mapping": { "status": "pending_sdk" },
  "distribution_ready": false
}
```

Hash exact bytes, preserving encoding and line endings. File paths use relative
forward-slash components; reject absolute/drive/UNC paths, `..`, alternate data
streams, duplicate destinations, case-colliding paths and link/reparse entries.
Bound file count and total expanded size. List every payload file; reject
unlisted payload. The record excludes itself to avoid a recursive digest. A
hash inventory checks consistency; it does not authenticate a publisher.

When the SDK is real, generate its required inner manifest from validated
evidence and the built plugin. Use the checker supplied for that SDK revision;
verify ABI/services, entry architecture, shader transforms, bindings and game
build claims. Do not emit `structural_only=true` as a workaround for unavailable
original identity: that field is a promise not to register hash keys in the
draft. [Draft §§7–8][draft].

For a future plugin ZIP/update record, keep the same tag-pinned download principle
as the current DLL feed. The outer record identifies the exact ZIP size/hash;
the inner manifest identifies the installed files. Pin a release source tag;
never put `/latest/` in a versioned payload URL. Stable discovery and prerelease
discovery are separate channels. Keep plugin version, settings-schema version,
prepared recipe and host ABI requirements as distinct identities.
[Current generator][runtime-manifest], [draft update rules][draft].

Updates run outside the game. Verify the full staged package before replacing
the active package; keep the previous verified version as rollback material.
Never delete or restore the settings subtree as part of a binary rollback.
If a newer settings schema cannot be read by an older plugin, refuse activation
or require an explicit compatible settings-generation selection; do not silently
downgrade user files. An explicit reset/import is separate from an update.

Automatic updates remain disabled until signed-byte format, trusted key
distribution/rotation, issue-time/replay policy and the prerelease feed are
agreed and verified. SHA-256 alone is not a signature. These signing details
are host/publisher agreement gates; no downloader or signer is justified now.

## Permissions and attribution gate

The runtime's GPL-derived code and EDHM assets are separate permission domains.
EDHM's current license states restrictions on modification, reuse and
redistribution; its exceptions name EDHM_UI v2/v3 and Nixpkgs distributions and
do not transfer general permission to a fork. Keep this as a release gate and
obtain the applicable author permissions before adapting or publishing assets.
The host's proposed MIT SDK does not grant rights to EDHM's shaders, themes or
configuration. [Maintainer-stated license][edhm-license],
[specific exceptions][edhm-exceptions], [draft §10][draft].

Record author, upstream path/revision, exact source digest, applicable terms,
permission scope and evidence for every distributed asset. Retain runtime
upstream notices; evaluate copied code under its own license. Do not bundle
game-original shaders, Microsoft compiler binaries, community themes or EDHM
replacement assets merely because a local preparer consumed them. A local
integrity receipt is not redistribution permission. An authored minimal sample
is suitable for a future public SDK demonstration while asset permissions remain
unsettled.

## Acceptance criteria and remaining external inputs

These are proposed observable checks for future implementation, **not tests
written or claimed to pass by this document**:

- A package update changes its owned binaries/defaults while a user's theme,
  editor metadata and runtime state survive byte-for-byte.
- A completed handoff imports selected scalar values with original row indices;
  packed colours use the editor's conversion and unsupported runtime expressions
  remain explicit exclusions. A partial batch never becomes active.
- Two mtime notifications for identical content cause one generation adoption;
  a same-size content change with unchanged timestamp is still detected.
- Failed save/commit keeps the previous generation active and new values
  pending. Concurrent theme editing and hotkey-state saving do not overwrite
  each other's files.
- Corrupt/unlisted/escaping payloads, wrong architecture, stale hashes and
  missing required capabilities prevent activation. An old compatible package
  can be selected explicitly without deleting settings.
- A no-op/theme-only refresh does not reload shaders. Shader changes require
  the restart policy of the actual host; draft v1 requires a restart.
- Runtime DLL discovery/update continues to use the original feed independently
  of native plugin discovery/update.

The SDK-independent decisions above are ready to review. Implementation of the
host manifest, actual editor destination selector, file-watch registration,
constant publication and installation still needs these external inputs:

1. A published SDK/header/checker and runnable host, with accepted service names,
   versions, block capacity, path ownership and settings notification semantics.
2. EDHM_UI maintainer agreement on an export/completion handoff and profile
   selection; current editor behavior was inspected, not modified or endorsed.
3. Author permissions for any adaptation/distribution, plus verified original
   shader/build evidence for hash-keyed replacements.
4. Agreed publisher/plugin identity, update channel, signing and rollback policy.

No generic backend/settings refactor is required to finish these design
decisions. Reuse a narrow numeric/transition module only when a real plugin or
tool is its second caller. Leave per-draw queries, presets with side effects,
scene detectors and resource writes in the current runtime until their future
host equivalent is concrete. [Rank 5](edo-next-readiness-2026-11-16.md#5-reuse-only-demonstrated-value-logic--conditional-on-a-consumer).

## Evidence

Runtime links pin investigation baseline `4ae92594`; their cited contracts
remain unchanged in preliminary source checkpoint `aeadf7a9`. EDHM_UI links pin
the public main revision inspected on 18 November. The host draft is a design
source, not SDK/runtime evidence; the main plan records its inspected live
revision, including later texture-tag additions absent from the saved text.

Local installed evidence was read only. `ThemeSettings.json` has 221,979 bytes
and SHA-256 `328a37804c8e5528387a2d2f971bbed89d1062de4832c3aeedf965a6fdc49daa`;
`XML-Profile.ini` SHA-256 is
`122a9fadf4d778e6bb9e2d1b909a2a8c25d5da62985d8fddfc42a2507c9855ab`;
`Startup-Profile.ini` SHA-256 is
`daee55280b1a76ea1c803d1f490fce7c61faf50c09d2cb5e0c31e653e0ac9740`.
These identify one inspected configuration, not authoritative defaults or game
build identity. No complete theme or replacement asset is included in this proposal.

[runtime-manifest]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/scripts/write-edup-manifest.ps1#L16
[runtime-feed]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/vendor/edup-runtime/manifest.json
[release-guide]: releasing.md#edup-updater-assets
[draft]: https://claude.ai/artifact/4DukqyMLdzLRSWgYRxH7Qc
[draft-status]: edo-next-readiness-2026-11-16.md#decision-and-evidence
[editor-conversion]: https://github.com/BlueMystical/EDHM_UI/blob/3f5807ac66b7ced8ad0a1741e412f3891f9f2bb1/source_v3/src/Helpers/ThemeHelper.js#L654
[editor-save]: https://github.com/BlueMystical/EDHM_UI/blob/3f5807ac66b7ced8ad0a1741e412f3891f9f2bb1/source_v3/src/Helpers/ThemeHelper.js#L793
[editor-retry]: https://github.com/BlueMystical/EDHM_UI/blob/3f5807ac66b7ced8ad0a1741e412f3891f9f2bb1/source_v3/src/Helpers/ThemeReloadSignal.js#L1
[runtime-watch]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/HackerDXGI.cpp#L540
[runtime-save]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/IniHandler.cpp#L5513
[runtime-persistence]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/PersistentSettingsState.h
[assignments]: https://github.com/Fenris159/EDHM_2DMigoto/blob/4ae925945879a897309992f58db6cc752f22289d/DirectX11/CommandList.cpp#L5484
[value-plan]: edo-next-readiness-2026-11-16.md#5-reuse-only-demonstrated-value-logic--conditional-on-a-consumer
[edhm-license]: https://github.com/psychicEgg/EDHM/blob/73588af9a1a17cf4aa66aca582afde9d98da60d5/LICENSE.md
[edhm-exceptions]: https://github.com/psychicEgg/EDHM/blob/73588af9a1a17cf4aa66aca582afde9d98da60d5/REDISTRIBUTION_EXCEPTION.md
