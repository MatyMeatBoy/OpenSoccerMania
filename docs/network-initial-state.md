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
cross-process logical identity. That initial inspection alone does not establish the types of all 214 words;
the provenance follow-ups below add the missing field-level evidence.

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
pair. At that stage, changed actor offsets `0xd0` and `0x148`, the three changed
global-header words and sixteen changed team-component words had not yet been
audited; the next follow-up below resolves their reference semantics. Stable
referent identities are still unverified. This is not a complete
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

## Follow-up: remaining reference semantics

Fresh assignment, copy and dereference sites account for the remaining 45 words
in the three components sharing the allocation-like delta:

- Actor `+0xd0` receives a descriptor loaded through resource reference `+0x2c`
  (`0x55a2b1..0x55a2c5`, stored at `0x55aa4d`); subsequent code treats it as an
  address. The 104-byte copy from actor `+0x6c` to `+0xe4` at
  `0x55b509..0x55b536` includes nested descriptor `+0x64`, copying actor
  `+0xd0` to `+0x148`. All 13 captured actors in each accepted run have nonnull,
  equal values at those two offsets. These account for 26 changed words.
- Global header offsets `0x20`, `0x24`, `0x28` correspond to globals
  `0x631e80`, `0x631e84`, `0x631e88`. Initialization derives all three from the
  team-array base `0x631e5c`: a 168-byte record stride, with `0x631e80` set to
  base plus 336 bytes. Both captures have `p80 == p84 + 0x150` and
  `p84 == p88`. They are record references and an alias, not three independent
  numeric gameplay quantities. Stable team identity must replace the addresses.
- Team serialization copies a 100-byte header and then 412-byte player records.
  Header `+8` is dereferenced as the player's pointer array at
  `0x47f988..0x47f990`. The observed group boundaries are 0, 2572 and 5144,
  with 6, 6 and 1 serialized player records. Their three changed header words
  are exactly the `+8` array references. This is the observed layout, not a
  general decoder or proof that all setup metadata is serialized.
- Each player record's `+0xc0` remains in the first 256 bytes copied by
  `0x485b50`. Code at `0x485803..0x485810` and `0x487b79..0x487b85`
  dereferences that field as a context object. These thirteen positions account
  for every other changed word in the team component. The thirteen context
  references are equal and nonnull within each captured process; equality of
  the referenced contexts across processes is not established.

Together with the earlier constructor/cleanup audit, all 214 changed words in
these three components now have reference provenance. This does not establish
a complete reference schema: equal fields, null fields, aliases and dynamic
referents outside the changed set still need coverage. Nor does it establish
portable IDs, equivalent referent contents or a valid restore representation.
No bytes are masked and no whole-state equality is accepted. The two changed
words in the conditional 16-byte component, its uncaptured flag and the distinct
inline RNG remain unresolved. Private receipts are
`local/netplay-initial-2026-11-17/remaining-reference-provenance.json` and fresh
instruction disassembly; arithmetic/alias checks also use both captured pairs.

## Follow-up: owned buffers and conditional flag

A second controlled Friendly pair (`c`, `d`) records the conditional flag before
and after every original component callback. It is 1 in both processes. Thus
`0x453200` takes its copying branch in this pair; its two changed bytes cannot
be classified as an unwritten destination merely from the earlier conditional
code inspection. The field meaning remains unresolved. The prior `a`/`b2` pair
still has no recorded flag and is not retroactively assigned one.

The same source-copy probe follows the ten identified owned-buffer fields for
each of 13 actors. Resource metadata reports 19 bones; all 130 regions per
process pass bounded committed/readable memory checks. Their recorded lengths
sum to 22932 bytes per process, and all 130 corresponding buffer byte spans
match. This does not prove all bytes were initialized, logically necessary or
covered by original replay serialization, nor compare complete resource graphs.
The independent inline RNG still differs (1224663864 / 1223669931).

Both frame-zero aggregates still contain 18 contiguous components totaling
28604 bytes with seed/shared RNG 999 and six controls. Both private processes
close with exit zero, input desktop unchanged and copied save hashes identical.
Private receipts are `local/netplay-initial-owned-2026-11-17/owned-comparison.json`,
its capture reports/verifiers and source-copy DLL; phase captures are under
`local/acceptance-2026-11-17/network-initial-owned-c/` and `network-initial-owned-d/`.
No production input, seeds or scheduler behavior changes. Investigation of the
copied conditional fields, stable reference identities and RNG ownership remains
necessary before initial logical equivalence can be accepted.

## Follow-up: conditional-byte writer provenance

The 16-byte component copies eight bytes from `0x62839c` and eight from
`0x6283bc`. Consequently, its changed offsets 4 and 12 map to byte fields
`0x6283a0` and `0x6283c0`, rather than two complete pointer-sized values.
Fresh inspection of the supported executable identifies two concrete writers:

- In initializer `0x452ff0`, the two saved-register pushes make
  `[esp + 0x10]` refer to original argument 2. Instruction `0x453047` loads
  that argument's low byte into DL; the loop at `0x453053..0x45306a` stores
  it at `0x45305f` to both fields with stride `0x20`. The same argument is
  used as an address earlier through EDI and `[edi + 0x30]`. This is a
  byte derived from an address argument, not a full serialized pointer.
- Reset `0x453280` first explicitly pushes incoming ECX, then loads DL from
  that newly written stack slot. Its corresponding stride-`0x20` loop writes
  the same two byte fields. Calling this an unwritten stack slot would be
  incorrect: its machine-level source is the saved incoming register. A normal
  C++ observer could clobber that caller-saved value before forwarding the call.

A fresh controlled Friendly run (`f`) uses the same teams, map, copied profiles
and four-pad assignments as the accepted owned-buffer pair. The private probe
records one initializer call from `0x40e27a`; argument 2 is `0x1362cd80`.
Both captured byte fields are `0x80`, matching its low byte. No reset call is
observed in this run, so that reset writer has static provenance only here.
The initializer-entry flag is not logged by this probe; the flag value of 1
before/after all 18 serializer calls must not be retroactively assigned to it.
The result is a runtime correlation with an identified writer, not a complete
dynamic inventory of all writes or proof of field meaning.

The first aggregate again contains 18 contiguous components, 28604 bytes,
frame zero, seed/shared RNG 999 and six controls. Its independent inline RNG
is 1701739931. The game closes with exit zero, unchanged input desktop and
identical hashes for all three copied saves. The observer's separate synthetic
contract fixture checks forwarding of entry volatile values, carry and SIMD,
and preservation of return registers, arguments and the return value.

The earlier attempt (`e`) is explicitly excluded: its reset signature guard
rejected an incorrectly guessed instruction encoding. The corrected guard uses
the actual bytes, including the explicit zero stack displacement. Both hooks
install in run `f`; no production hooks, input, seeds or scheduler change.

These findings narrow the provenance of the remaining two differing bytes.
They do not prove those fields are unused, padding, portable IDs or safe to
normalize. No bytes are masked and no initial logical equivalence is accepted.
Private evidence is under `local/netplay-conditional-2026-11-17/`:
`conditional-provenance.json`, `capture-f.json`, `prior-field-values.json`,
fresh writer disassembly and the observer contract fixture. Runtime phase images
are under `local/acceptance-2026-11-17/network-conditional-f/`.
