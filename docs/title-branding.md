# Private title branding

Both regional title graphics are present in the inspected owner-provided library:
`Mania/CD/FrontEnd/BGs/Title_SM.BMB` and `Title_FM.BMB`. No game graphics or
archives are included in this repository or intended for release packaging.

`available_brands(entry)` returns present `soccer` / `football` identifiers.
`load_title_image(entry, brand)` returns a native 640 x 480 Pillow RGBA image.
`load_title_tiles(entry, brand)` returns named `TitleTile` objects for inspection.
Failures raise `TitleAssetError`; the launcher can fall back to generated text.
All operations read only the selected library entry and hold decoded images in
memory. They never change originals or create a disk cache.

## Format evidence and bounded support

The inspected files contain a DWORD count, an `IDX2` chunk with WORD ordinal and
null-terminated ASCII names (terminated by 0xffff), followed by `INFO`, `IMAG`,
optional `CLUT`, and `END ` chunks. INFO contains little-endian WORD width/height
at byte zero and DWORD depth at byte 20. Supported payloads are uncompressed
24-bit BGR, 32-bit BGRA, or 8-bit indices with RGBA CLUT entries. Palette alpha
is preserved, including partial transparency; out-of-range indices fail.
The inspected assets exercise 24-bit and 8-bit decoding; 32-bit decoding is
covered by its declared byte format but has not been observed in these titles.

The Soccer file names tiles `MakeBG_10` through `MakeBG_17`; Football uses
`MakeBG_2` through `MakeBG_9`. Both have four 256 x 256 tiles followed by four
128 x 128 tiles. Assembly uses the first four in column order at (0,0),
(0,256), (256,0), (256,256), then the small tiles at x=512 and y=0,128,256,384.
The 640 x 512 texture canvas is cropped to 640 x 480. This layout was validated
with private previews: continuous blue background, complete red regional title,
yellow Mania text, ball and pirate. Tile seams align without stretching. INFO
itself does not encode screen placement: this is an explicitly recognized title
layout, not a generic BMB composition algorithm. Unknown names/dimensions fail
closed. Individual tiles remain available for investigation.

Limits: 16 MiB archive, 64 tiles, 2048 per dimension, and 4 million decoded
pixels total. Chunk lengths, index order, palette lengths, pixel lengths,
terminators and trailing bytes are validated before accepting a result.
Tests use only generated solid-color data. Private previews, if generated
manually, must remain under the ignored local library and never be distributed.

## Generated widescreen extension

`assets/backgrounds/title-extension.png` is a newly generated 2172 x 724 RGB
background, created with the built-in OpenAI image generation tool on
2026-11-16. It contains only a navy-to-blue vertical gradient. A private decoded
Football title was provided solely as a color reference; no original title
pixels, letters, logos, ball, character, or other game graphics are included
in this published asset. This generated project artwork is separate from the
owner-provided copyrighted title assets and may be packaged with the launcher.
No third-party stock asset was used; the repository's project license governs
this project asset, subject to applicable generated-output rights.

Generation prompt: create a background-only panoramic blue gradient for title
sidebars, matching the reference edge colors from RGB(15,19,81) at the top
through (17,53,127) at mid-height to (20,87,174) at the bottom; keep it even
horizontally, with no objects, text, logos, trails, black borders, or watermark.
The tool returned a 3:1 canvas rather than the requested approximate 32:9.
Because it is a featureless gradient, the runtime may fit it to 16:9, 21:9, or
32:9 behind the intact 4:3 title without geometric distortion of any subject.

Visual inspection confirms a clean blue-only gradient and no game artwork.
Generated colors are approximate, not pixel-exact to the private title edges:
center samples are RGB(10,14,95) at top, (11,53,142) at mid-height, and
(16,100,189) at bottom. Runtime edge blending should use privately decoded
edge colors outside the original image rectangle to ensure continuous joins.
The original 4:3 title center must be drawn separately, never stretched,
regenerated, or overwritten by the extension.

## Private runtime sidebar compositor

`prepare_title_presentation(entry, brand, generated_background_path, cache_root)`
in `opensoccermania.title_presentation` returns a metadata dictionary and writes
only a private sidebar BMP plus JSON. `cache_root` must be a strict subdirectory
of the selected library entry. The cache key includes decoder/compositor version,
brand, source archive SHA-256 and generated-background SHA-256. Writes use sibling
temporary files and atomic replacement; a cached BMP is length/hash checked.

The BMP is 2048 x 512, 32-bit BGRA with BITMAPV4 alpha masks. Its central
4:3 rectangle is `[682, 0, 1365, 512]`, exclusively transparent zero RGBA.
Original title pixels are never copied into that rectangle. Only the two outer
sidebar regions are opaque. For each row, 96 pixels outside each join blend
from the exact nearest original title edge RGB at the boundary into generated
background color. The far end of each strip is entirely generated color.
Native code must draw sidebar quads outside the original title rectangle and
retain the game's original title rendering unchanged. This runtime-derived BMP
contains private edge-color samples and must never enter a release package.

`backgroundBmp`, `backgroundSize`, `centerRect`, and `centerUV` describe the
4:1 source panorama. Centered horizontal UV cropping supports 16:9, 21:9 and
32:9 while retaining the native 4:3 title region. Use independent left/right
UV mappings anchored to `centerUV` when mapping an exact 4:3 center, since the
683-pixel texture center is integer-rounded. `originalSize` remains `[640,480]`.
`tileFingerprints` includes every decoded tile's name, width, height and sixteen
interior `(x,y,rgb)` samples in a regular 4 x 4 grid. Fingerprint RGB is independent
of native render texture format. This fingerprint metadata also stays private.

Synthetic tests validate alpha/zero center bytes, exact top/middle/bottom edge
colors on both sides, full generated color at blend endpoints, tile samples,
source/background invalidation, private cache confinement and invalid inputs.

## Native D3D8 integration helper

`native/title_background.h` is independent of the proxy integration. It accepts
only the exact compositor BITMAPV4 shape and masks, validates every center pixel
as zero RGBA and every sidebar pixel as opaque, flips the bottom-up rows, then
uploads one managed A8R8G8B8 texture. Input is bounded to the expected BMP length.
Configuration is `UI.TitleBackgroundPath`, written by the launcher as an ASCII
path relative to the selected private entry. Native loading resolves it against
the configuration directory, independently of changes to the working folder.

Integration calls:

- `osm::start_title_hooks(config, log_path)` before the existing global MinHook
  enable step. It creates both hooks only for the verified executable and
  byte-matching image draw, flush, transition, and two title callsites.
- `osm::title_background.set_device(device)` when the device is created.
- `osm::title_background.release()` before device destruction/reset; set the
  device again after successful reset. The helper does not own a device reference.
- COM Release calls that leave references alive retain the device binding; an
  ordinary reference decrement must not disable subsequent title rendering.
- `osm::reset_title_frame()` at Present/reset boundaries as an extra stale-marker
  guard. Device and hook operations must remain on the game's render thread.

The image draw hook at 0x415850 marks a title frame only when its direct caller
returns to 0x4f2f86 or 0x4f30a1. This excludes disclaimer and other image screens.
The transition entry hook at 0x4e2940 consumes that marker, explicitly flushes
original title/prompt batches via 0x415320, immediately draws the sidebar quads,
then calls the original transition update/draw/present. No central title draw
is replaced. The helper uses full target dimensions and draws only the two
regions outside centered 4:3. It preserves a D3DSBT_ALL state block plus explicit
viewport and stream-zero references (DrawPrimitiveUP can unbind the stream).
It never changes render targets. Shader, sampler, depth, stencil, fog, cull,
blend, clipping and texture-stage states changed for the draw are restored.

Proxy geometry hooks must bypass additional aspect scaling for these physical
XYZRHW sidebar quads. Their hook callsites can be excluded specifically, or
integration can route this helper through the original device dispatch.
The helper itself issues no BeginScene/EndScene or Present calls.

Transition limitation: the original random STAR/CIRCLE/SHUTTERSG/BASIC wipe may
cover only 4:3. Insertion before transition rendering preserves ordering but
does not prove those masks cover the added sidebars. Full-width wipe coverage
requires separate verified integration; prompt blinking values are not fade
opacity and must not drive sidebar brightness.

`tests/native_title_background.cpp` compiles and passes under the project's
i686 LLVM-MinGW toolchain. It tests bounded BMP decoding, row orientation,
center/alpha rejection, narrow/wide geometry and precise context rejection.
`tests/native_title_render.cpp` also passes against the real D3D8 backend on an
isolated Win32 desktop, with the input desktop unchanged. It verifies sidebar
pixels on both sides, exact 239/240 and 1679/1680 joins at 1920 x 1080, preserved
central output, and restored viewport/fog/blend/stream-zero state. No game or
visible desktop was launched by this helper task. Actual game transition
coverage remains unverified.

`osm::title_background_drawing` is an inline thread-local scoped flag covering
the entire draw, including target-description lookup. The proxy must bypass
its logical surface descriptions and viewport/DrawPrimitiveUP transformations
while this flag is set. This allows the helper to operate in physical pixels.
`Graphics.TitleExtension=0` prevents creation of both context hooks. The first
actual title-triggered draw writes one bounded log line with result and physical
target dimensions; subsequent frames do not produce repeated draw logs.

The integrated bridge subsequently reached the real title in an owned isolated
game probe and logged `first-draw result=ok physical=1920x1080`. The earlier
integration failed before target lookup because an ordinary device-reference
release cleared the helper binding; retaining live-device bindings and resolving
the BMP against the configuration directory corrected this reproduced failure.
The probe's input desktop stayed unchanged. Its timed shutdown needed forced
termination of its own process; clean shutdown is not established by this run.
The captured preceding disclaimer retained black sidebars, as intended by the
precise title-only gate. Full title/wipe visual acceptance remains open.
