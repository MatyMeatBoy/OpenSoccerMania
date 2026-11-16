# Regional profiles and text language

`[Game] Edition=SOCCER` selects Soccer Mania naming and regional assets.
`Edition=FOOTBALL` selects Football Mania. `[Game] Language` independently selects
the text locale. Explicit Soccer with Spanish, or Football with American English,
therefore keeps the requested text language. When Edition is absent or invalid,
AMERICAN language preserves the historical Soccer default; other languages default
to Football. The launcher should write an explicit Edition for independent choices.

The portable registry exposes only the selected Soccer Mania/Football Mania key,
so the original app-name selection follows Edition. Normal virtual registry
Language queries still return the configured text locale (or the existing legacy
value for AUTO).

## Version-gated regional dispatch

`osm::start_edition_hooks(config_path, log_path)` installs the regional comparison
hook during the existing MinHook setup, before enabling hooks. It requires
`[Graphics] VerifiedExecutable=1`, the expected executable base, sufficient image
bounds, committed image memory, the exact 24-byte comparison-function signature
at 0x542810, and all four exact call instruction signatures.

Only these comparison return addresses change their result:

| Return address | Regional selection |
| --- | --- |
| 0x44a49e | pc_cd_ntsc / pc_cd_pal |
| 0x4eed38 | Cached language PAL/NTSC resources |
| 0x4f2e7e | TitleSM / TitleFM |
| 0x4f9f1f | INTRO_US / INTRO |

These identified regional branches receive comparison result zero for Soccer or
one for Football. All other comparisons call the original function. Executables
with unknown signatures receive no regional comparison hook.

The early pc_cd selection can run before MinHook initialization. For that one
registry lookup, the same executable gate plus a bounded eight-frame stack match
for outer return address 0x44a493 supplies AMERICAN/ENGLISH as a regional selector.
Every other Language lookup remains the actual text locale. This avoids changing
the shared registry language globally.

This block changes resource dispatch and branding. It does not change the logic
rate, timing constants, clock, or the verified 50 Hz simulation behavior.

## Verification

`tests/native_edition.cpp` exercises explicit/fallback selection, locale
independence, all guarded call sites and neighboring addresses, exact signature
acceptance and rejection, short images, disabled verification, mocked early
stack capture, eight-frame limits, portable app-name selection and actual
virtual-registry text output. The native executable runs without launching the
game or synthesizing desktop input. End-to-end regional asset appearance and
all nine text locales still need game-level verification on the owner's assets.
