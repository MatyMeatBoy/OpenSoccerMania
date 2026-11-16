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

`open_disc_image(path, extractor=None, max_bytes=4 * 1024**3)` is a context manager
that yields a temporary extracted directory. It requires an installed
[7-Zip](https://www.7-zip.org/), whose official supported formats include ISO and
UDF. The importer finds `7zz`/`7z` on PATH or the Windows Program Files installation;
an explicit executable path may also be supplied. Nothing is downloaded or
installed automatically. The tested local dependency was the installed Windows
7-Zip command-line executable. Existing `sources.mounted_iso()` remains available
for Windows read-only ISO mounting without 7-Zip.

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
describes CD-ROM sector structures. Extraction lists entries first and streams each
file through stdout to a validated destination; 7-Zip cannot select filesystem
output paths or create links. Path traversal, Windows device/stream names, case
collisions, listed links, symlinks and Windows reparse points are rejected.

Image files and total extracted payload are limited to 4 GiB, listings to 16 MiB,
entries to 100,000, and each extractor process to 120 seconds. Stdout is streamed
with an exact byte cap; a watchdog terminates timed-out processes. Extraction uses
hidden subprocesses without prompts or desktop interaction. Staging is removed on
success or failure. Selected sources should remain unchanged by other programs
while an import is running; this is not a filesystem snapshot or an adversarial
concurrent-writer sandbox.

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

## Reusing an owner-supplied graphics backend

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

No backend is bundled, downloaded, or copied from an unselected original game
folder. Discovery uses existing owner-provided private library copies only.
The DLL and configuration source bytes are preserved. No global Windows graphics
configuration or original installation is changed.

The launcher now performs this backend check inside its import worker, before
portable setup and before presenting the import as ready. A source without a
wrapper/configuration pair reuses a recognized backend from the private library.
If no donor exists, the launcher asks for the owner's x86 dgVoodoo D3D8.dll and its
matching configuration; it uses an adjacent dgVoodoo.conf when present. Cancelling
leaves the copied private entry available for a later retry. No installer or
backend is fetched. An incomplete existing graphics setup receives an actionable
error and its files remain unchanged. Headless tests exercise automatic reuse,
explicit selection, missing-backend handling and preservation; no GUI is opened
by these tests. An optional local test also validates/copies the actual private
owner backend into a disposable directory and checks source hashes afterward.

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
brands decoded at 640 × 480. Their configured edition/language, supported patched
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
