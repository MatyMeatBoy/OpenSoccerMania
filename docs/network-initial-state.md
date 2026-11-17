# Controlled initial-state comparison

## Matching setup and capture boundary

Two fresh private-desktop processes select Friendly, LEGO All Stars at home,
Lawbringers away and Western Junction. Both use byte-identical copies of the
three owner profile/save files, including OPTIONS.SAV, and identical wrapper
settings and diagnostic DLL. No gameplay option is changed. Captures verify the
same teams, field and controller assignment: Joypads 1/2 home and 3/4 away.
This is a controlled visible setup and profile match, not a proof that every
internal initialization input is identical.

A private source-copy DLL wraps all 18 registered bucket-0 save callbacks and
save dispatcher `0x4812e0`, with executable signatures checked before installation.
It records the first bucket-0 save only. Original arguments, return pointers,
seed behavior and Win32 last-error state are preserved; no simulation input or
seed is overwritten. Each callback's returned span is bounded before copying it
to a private diagnostic file.

Both accepted captures contain exactly 18 components, matching the engine's
expected count, at original recording frame zero with six control records.
Counter seed and shared RNG are both 999. Each component's output pointer is
the next component's input pointer; their files concatenate exactly to the
28604-byte aggregate. This verifies the captured serialization spans, not that
every byte is a written logical field or every game field is represented.

The accepted runs are `a` and `b2`. An earlier `b` timed navigation sequence
entered a different menu path; it was closed cleanly and explicitly excluded.
The accepted second run checks its menu phases before continuing. Different
menu backgrounds and navigation elapsed times are retained, not hidden.

## Raw component differences

| Save callback | Captured bytes | Different bytes |
| --- | ---: | ---: |
| `0x518880` | 15912 | 520 |
| `0x480aa0` | 216 | 6 |
| `0x47f950` | 5656 | 32 |
| `0x453200` | 16 | 2 |
| Other 14 components | 6804 | 0 |

There are 560 different bytes in total. In the first three differing components,
all 214 differing aligned 32-bit words have the same arithmetic difference:
`a - b == 0x000fe000`. This is consistent with process-local allocation
relocation, not a verified pointer schema or permission to normalize all such
values. In the actor component the 195 changed words occupy the same 15 offsets
in each of 13 chunks of 1224 bytes:
`0x28, 0x2c, 0xd0, 0xd4, 0xdc, 0xe0, 0x148, 0x14c, 0x154, 0x158, 0x15c,
0x1cc, 0x1d4, 0x1d8, 0x204`.

Fresh disassembly explains why references can be retained. `0x518880` iterates
actor objects with input stride `0x368`; its helper `0x559bb0` copies the full
`0x368`-byte object before auxiliary data. The helper explicitly dereferences
pointer fields such as `+0x2c`, `+0x1d4` and `+0x1d8`. This supports inspecting
field types and referents, rather than treating a raw aggregate hash as a
cross-process logical identity. It does not establish the types of all 214 words.

The two remaining changed words are offsets 4 and 12 of callback `0x453200`:
`0x40` versus `0xc0` in this pair. That callback writes its 16 bytes only when
`0x628050 == 1`; otherwise it advances the destination by 16 without writing.
The branch flag was not captured, so these differences remain unclassified.
They must not be dismissed as padding or accepted as gameplay divergence yet.
An equal component can likewise contain reserved/unwritten bytes; byte equality
of the other 14 components is not complete logical-state acceptance.

The separate inline RNG at `0x6276e8` differs at the same capture boundary:
1762562697 versus 3266180549, despite matching shared RNG and counter seed.
Its effect on the requested synchronized simulation is not established here.
All generator state domains still require explicit ownership and verification.

## Verification and next work

Both accepted processes close with exit zero, the offscreen runner confirms
unchanged input desktop, and all three copied save hashes remain identical.
The same diagnostic DLL SHA-256 is used for both. Private receipts are under
`local/netplay-initial-2026-11-17/`: `capture-a.json`, `capture-b2.json`,
`comparison.json`, `verify_capture.py`, fresh layout disassembly and source-copy
DLL. Binary components and phase captures remain in their isolated entries and
`local/acceptance-2026-11-17/network-initial-a/` / `network-initial-b2/`.
The excluded run has an explicit `network-initial-b/excluded.json` receipt.
No owner assets or private state dumps are distributed.

Next work must capture the conditional flag, establish typed reference IDs and
written ranges for these components, and compare complete logical fields without
masking unexplained differences. Matching frame-zero state is still unproven;
per-update confirmed input, cross-process step results, scheduler waiting and
recovery remain required before playable online synchronization can be accepted.
Production native code and the published pre.7 baseline are unchanged.

## Follow-up: changed actor reference provenance

Fresh constructor, initialization, cleanup and use-site inspection identifies
reference semantics for 13 of the 15 changed offsets in each observed actor
chunk. This is stronger evidence than the common arithmetic relocation delta:

| Actor field offsets | Inspected provenance |
| --- | --- |
| `0x28` | Resource argument stored at `0x559905`; the next instructions dereference that argument to obtain the reference below. |
| `0x2c` | Resource-derived reference stored at `0x559913`, then dereferenced by the actor serializer helper at `0x559bc0..0x559bc7`. |
| `0xd4`, `0x14c`, `0x1cc` | Nested block `+0x68`: allocator result stored at `0x559833`, then passed to the allocator's release entry during actor cleanup. |
| `0xdc`, `0x154`, `0x1d4` | Nested block `+0x70`: allocator result stored at `0x559852`, with matching cleanup release paths. |
| `0xe0`, `0x158`, `0x1d8` | Nested block `+0x74`: allocator result stored at `0x55980c`, with matching cleanup release paths. |
| `0x15c` | Separately allocated 24-byte buffer stored at `0x5597ca` and released through the cleanup path at `0x559af5`. |
| `0x204` | Copy of reference `+0x2c` at `0x55a776..0x55a779`. |

Constructor `0x559770` passes the actor's nested bases `+0x6c`, `+0xe4` and
`+0x164` to the same allocation helper `0x5597e0`. Adding the nested field
offsets produces the nine corresponding actor offsets above. Cleanup
`0x559a20` loads each owned buffer, checks it for null, and passes it to the
allocator object's `+4` release entry. These field values are references,
not scalar gameplay quantities to compare as raw addresses. The captured
`+0x2c` and `+0x204` values are also nonnull and equal in all 13 actor chunks
of both accepted runs, matching the inspected alias assignment.

The classified offsets account for 169 of the 195 changed actor words in this
pair. Changed actor offsets `0xd0` and `0x148` still need their own provenance
and referent identity; the three changed global-header references and sixteen
changed team-component words also remain to be audited. This is not a complete
pointer inventory: equal/null reference fields can exist outside the changed
set. No capture bytes are altered or masked by this classification.

A portable state representation must replace verified references with stable
identities and account for their relevant contents, lifetime, aliases and null
values. Simply dropping the 169 typed words would not prove logical equality or
produce a valid restore format. The conditional flag and distinct inline RNG
requirements above remain open. Private static receipts are
`local/netplay-initial-2026-11-17/actor-reference-provenance.json` and its fresh
disassembly; selected instruction anchors and release paths are checked against
the supported owner executable. Production code and seeds remain unchanged.
