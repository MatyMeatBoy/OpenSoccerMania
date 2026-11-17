# Importing owner-provided game copies

Imports read the selected originals and create a separate, transactional private
library entry. Disc installers and game executables are never run during import.
A source can be imported even when its executable version does not support native
hooks: runtime features must continue to use the exact executable SHA-256 gate.
An `MZ` signature identifies a potential executable; it does not prove compatibility.

## Disc image formats

`opensoccermania.disc_images.probe_disc_image(path)` checks content without mounting,
launching programs, or changing the image. Its result includes `supported`, `reason`,
`data_path`, `sector_size`, and `data_offset`.

| Input | Supported content | Limits |
| --- | --- | --- |
| ISO, IMG | ISO9660 in 2048-byte sectors | UDF-only, compressed and encrypted images are unsupported. |
| BIN, MDF | ISO9660 in 2048-byte sectors, or a single raw data track with 2352-byte MODE1 or MODE2 Form 1 sectors | Audio, 2448-byte subchannel sectors, track offsets and multisession layouts are unsupported. |
| CUE + BIN | Exactly one adjacent BINARY file, one MODE1/2048, MODE1/2352 or MODE2/2352 track, INDEX 01 00:00:00 | Multiple files/tracks, audio, pregaps, nonzero indexes and paths outside the descriptor folder are rejected. |
| NRG | An ISO9660 data volume starting at sector zero, in one of the layouts above | NRG session metadata is not interpreted. Unrecognized variants require an external conversion. |
| MDS | No descriptor parser | Select the paired MDF explicitly for content probing. |

Extensions alone never establish support. A successful layout probe can still fail
import if a sector, filesystem entry or extraction is damaged. This is deliberately
not a universal converter. Unsupported variants should be converted with an
owner-chosen read-only disc utility to a separate ISO; retain the original dump.

`open_disc_image(path, extractor=None, max_bytes=4 * 1024**3)` yields a temporary
extracted directory using the included ISO9660/Joliet reader. Normal imports do
not search PATH, require installed utilities, start an extractor process or
perform a network download. A developer may explicitly pass an extractor for
comparison, but that optional path is not required by the portable launcher.
Windows read-only mounting remains a separate developer API; normal launcher
imports always use the included reader without mounting or shell commands.

```python
from opensoccermania.disc_images import open_disc_image, probe_disc_image
from opensoccermania.sources import discover, prepare

probe = probe_disc_image(owner_selected_image)
if not probe.supported:
    raise ValueError(probe.reason)
with open_disc_image(owner_selected_image) as extracted_root:
    source = discover(extracted_root)
    prepare(source, new_private_library_entry)
# The extracted tree and any normalized raw-sector ISO have now been removed.
```

Raw conversion uses the ISO volume size and validates every copied sector's sync,
mode and MODE2 subheader. It does not rewrite the image or use a virtual drive.
[ECMA-130](https://ecma-international.org/publications-and-standards/standards/ecma-130/)
describes CD-ROM sector structures. The built-in reader validates volume and
directory records, both-endian fields, extents, recursion/count/byte limits,
Joliet names, path/device names, collisions and unsupported link/record forms.
Sources remain read-only; temporary output is removed on success or failure.
The optional explicitly selected external-extractor path retains its bounded
stdout, watchdog and validated destination behavior. It is never selected
implicitly for a normal portable import.

## Asset-only copies and explicit private repair

`discover(path)` rejects missing, damaged and zero-filled executables.
`discover_assets(path)` instead returns `AssetSource(root, data, languages, voices)`
with `runnable=False`. This exposes usable assets without presenting that original
copy as playable. Asset sources cannot be passed directly to `prepare()`.

An owner can explicitly select another intact source for its executable and adjacent
runtime files:

```python
from opensoccermania.sources import discover, discover_assets, prepare_with_executable

assets = discover_assets(owner_selected_asset_copy)
donor = discover(owner_selected_intact_copy)
prepare_with_executable(assets, donor, new_private_library_entry)
```

This copies the selected assets and donor runtime into a new private entry. Neither
original is modified. `opensoccermania-source.json` records both source paths,
executable hash and `asset_repair=true`. Destination paths within either original
source are rejected; existing library entries are never overwritten. The copied
executable hash is checked before the private entry becomes visible. Mixing
regional assets and another executable does not establish language, save-file or
hook compatibility; the donor executable's exact hash still determines runtime
support. No proprietary game files belong in the public repository.

## Included graphics runtime and legacy backend APIs

`opensoccermania.backends.find_library_backend(library_root)` returns a `(dll, conf)`
path pair, or `None`. It inspects at most 200 immediate entries, each requiring the
private import manifest `opensoccermania-source.json`. It selects
`d3d8_backend.dll` only when the current `d3d8.dll` matches the entry's bridge hash
marker; otherwise an entry without a bridge marker may supply its original
`d3d8.dll` and `dgVoodoo.conf`. A mismatched bridge marker disqualifies that entry.
Symlinks, junctions, reparse points and paths outside the library are rejected.

The bounded parser requires a PE32 i386 DLL exporting `Direct3DCreate8` and
recognizable dgVoodoo identity bytes. It rejects OpenSoccerMania bridge identity,
wrong architecture, truncated headers and forwarded factory exports. Packed
backends may legitimately export functions into mapped virtual sections. Parsing
never loads or executes a DLL. Identity bytes are a conservative product check,
not a cryptographic trust signature or a guarantee of graphics compatibility.
DLL input is capped at 32 MiB; configuration/manifest input at 1 MiB each.

`install_backend(entry, dll, conf)` requires an existing private import manifest
and separate source files. A fresh entry receives `d3d8.dll` and `dgVoodoo.conf`;
normal bridge installation can subsequently preserve that DLL as
`d3d8_backend.dll`. The helper returns `True` when installed and `False` unchanged
if any graphics DLL, backend, configuration or bridge marker already exists.
It never merges or overwrites an existing graphics setup. Files are staged
privately, published using atomic exclusive hard links on the same volume, and
newly published files are rolled back if publication fails. This requires a
filesystem supporting hard links. The two-file operation is rollback-safe for
reported errors; it is not a crash-consistent filesystem transaction.

Since pre.3 the launcher uses `bundled_backend(root)` and `ensure_backend(entry,
root)`: exact pinned dgVoodoo 2.87.6 files from the dedicated game pack provision
new private imports automatically. Existing complete graphics setups remain
preserved, and missing private files can be filled without a component dialog.
There is no library-donor dependency on a fresh PC. Missing/damaged package files
report a full portable re-extraction, not a separate DLL/configuration download.
Vendor originals in the pack remain unmodified; only private copies receive
selected graphics settings. Legacy `find_library_backend`/`install_backend` APIs
remain for compatibility and developer checks, not a required user workflow.

## Actual owner-provided folder checks for pre.2

A bounded local check imported all three supplied folders into separate disposable
private entries, copying approximately 1.26 GB in total. It used the actual import,
backend and version-locked portable setup APIs, generated profiles, and decoded
both Soccer and Football title brands in memory. No game, installer or desktop UI
was launched. The private evidence summary is retained outside Git at
`local/pre2-import-validation.json`.

| Owner-provided folder | Executable selection | Graphics backend handling |
| --- | --- | --- |
| ElectronicArts-LegoSoccerMania | Original remains asset-only; an explicitly selected intact nested Football copy supplied its private runtime. | Validated existing private-library backend copied into the disposable entry. |
| lego FOOTBALL MANIA | Intact executable in its known nested installer layout. | Validated existing private-library backend copied into the disposable entry. |
| Lego Football Mania Dutch | Intact root executable. | Its complete supplied wrapper/configuration pair was preserved and validated. |

All three entries exposed nine text languages and eight voice folders. Both title
brands decoded at 640 Ã— 480. Their configured edition/language, supported patched
executable hash, native/input bridge copies and profile settings passed the check.
All 62 protected original/backend file hashes matched afterward, and every
disposable entry was removed. These results establish import and profile setup for
these inputs; they do not establish gameplay, full-match timing, physical
controller acceptance or universal disc-image compatibility.

The Dutch cleanup exposed Windows read-only attributes retained by `copy2` on
private message files. Failed-import cleanup now validates the exact created
`.import-*` staging path under its destination parent and rejects links/reparse
points before deletion. Only a failed unlink with `PermissionError` may clear the
read-only bit on a regular private file with no extra hard links inside that tree,
then retry once. Original paths and attributes are preserved. If cleanup still
fails, its staging path and the original import error are reported together;
cleanup errors are not suppressed. Focused Windows tests reproduce a real
post-copy import failure with read-only input, verify original hashes/attributes,
and verify staging removal and junction refusal.

## Read-only executables and interrupted startup patches (pre.4)

ISO media can mark Game.exe read-only. Its copied private executable can retain
that flag even though the library directory is writable. Startup patching now
retries replacement after clearing only that private executable's read-only bit.
It requires the private import provenance, rejects symlinks/reparse points and
extra hard links, and checks separation from both recorded original sources.
Owner source files, private original backups and Windows ACL permissions are not
changed. Normal read-only copies do not require administrator privileges.

A failed executable replacement leaves a verified original backup and a complete
recovery record. Retrying the import accepts either the exact original bytes or
the exact derived patched bytes; an original executable plus a valid pending
record resumes safely. Old valid records from the interrupted import are upgraded
when retried. Every application/restoration validates the backup, expected edits
and derived hashes; a foreign executable, corrupt backup or inconsistent record
is refused. Restoration can also be retried after interruption. Existing backup
bytes are never overwritten. A genuine permission/sharing failure reports the
private path and advises closing the game/checking that private folder, while
preserving the state needed for retry. Deleting the complete local library is not
needed to recover a valid interrupted patch.

A disposable pre.4 check also used the two actual intact owner executables without
copying or launching the game data. Read-only private copies underwent an injected
replacement denial, legacy-record recovery, repeated application and restoration.
Both original executable hashes and Windows attributes remained unchanged, as did
the private original backup bytes; all disposable copies were removed. The ignored
summary is `local/pre4-patch-validation.json`. This verifies patch transactions,
not gameplay or acceptance on the reporting PC.
