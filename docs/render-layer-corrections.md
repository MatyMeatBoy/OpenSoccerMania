# Screen-space layers and tiled images

The owner reported a sky ending at one third of the output width, shadows and effects constrained to the old frame, and thin seams across tiled title artwork.

## Identified mechanisms

- The sky used an `XYZRHW` vertex buffer with x coordinates from 0 to 640, even though the physical target was 1920 pixels wide. Unlike transformed geometry, these pretransformed positions bypass viewport scaling. The captured buffer used FVF `0x1c4`, stride 32 and six vertices per layer.
- `GetVertexShader` returns either an FVF or an opaque programmable-shader handle. Testing only the position mask incorrectly classified observed handles 5 and 53 as screen vertices. That could apply HUD/raster corrections to scene geometry. The bridge now excludes registered programmable shaders and reserved-bit values; shader registration is updated on creation/deletion.
- The title is assembled from texture tiles. Applying geometry MSAA and forced filtering to these image edges produced visible joins at tile boundaries. A same-running-title capture showed the joins before the sprite adjustment and their removal afterward.

## Corrections

Pretransformed vertex-buffer draws now receive the same logical-to-physical raster mapping as immediate draws. World-projected layers fill the scene output; flat HUD layers retain their proportions. Indexed conversion preserves the base vertex, rebases indices and restores stream/index bindings after the draw. Read-only lock failure preserves the original draw and emits a bounded fallback diagnostic.

The captured sky also uses depth values slightly beyond 1 with reciprocal-w values near zero. Hardware vertex processing clips that data even with the depth buffer disabled. When the layer matches this background pattern and depth testing is disabled, the bridge normalizes its depth to the far boundary. Scene fog is temporarily disabled for that background draw and restored afterward. Hardware readback tests reproduce the black result before these adjustments and verify color, repeated texture coordinates and fog restoration afterward. A real isolated space-map menu shows its nebula and stars across the output.

### Replay and horizon follow-up

A later live pirate-map trace exposed sky vertices with RHW ranging from `-0.000011315` to `0.000004600`. The earlier positive-only predicate rejected the whole layer. Simply accepting the layer and normalizing depth was insufficient: reconstructing negative homogeneous W still caused D3D11 clipping. The corrected path keeps raster W positive and transports the original signed perspective through projected texture coordinates `(u*rhw, v*rhw, rhw)`. It restores the original shader, texture-transform flags and matrix after drawing. It does not replace sky textures with a flat fill.

A low-angle trace exposed another boundary case: one layer's depth ranged from `0.9998924` to `1.0001178`. Requiring every vertex to exceed 1 excluded the whole layer. Background eligibility now accepts finite depths near the far boundary on either side, still requiring disabled depth testing and small nonzero signed RHW. Flat HUD vertices and ordinary depth-tested geometry do not qualify. Color-only background layouts receive the same depth/fog handling.

Synthetic hardware tests first reproduced black output for color-only, signed-horizon and far-boundary cases. The corrected bridge passes indexed/base-offset, non-indexed, repeating-texture, immediate and indexed-immediate paths. The signed-horizon test checks perspective-correct texture samples analytically, rather than expecting an affine stretch; tests also verify shader, texture projection, scene fog and binding contracts. All tests run on an isolated desktop. Remaining all-map/replay acceptance is separate from these focused regressions.

WRITEONLY buffers are not read back. Bounded CPU mirrors observe successful writes, preserve partial updates, invalidate discarded regions and reject failed unlocks. Mirrored data is independent of GPU mappings after Unlock. The total mirror budget is 32 MiB; unavailable data falls back without inventing geometry. Isolated game traces show mirror hits and zero screen-buffer GPU reads.

The camera stores horizontal FOV, so changing only its aspect ratio caused vertical cropping. Hor+ now converts horizontal FOV to preserve the vertical view. The source lens is restored after building the active camera to avoid cumulative expansion across frames. Tests cover 4:3, 16:9, 21:9 and 32:9 lens behavior.

The sprite adjustment disables geometry edge MSAA for 2D layers, clamps texture edges and avoids anisotropic filtering on those layers while preserving original point sampling. Previous state is restored after each draw; 3D MSAA and filtering settings remain available. The launcher exposes this as **Fix seams in menus and tiled images**.

Tests cover the observed shader-handle collision, captured sky depth/rhw values and raster endpoints. Local captures are private under ignored `local/render-evidence/`; no original artwork is included in source. Wider scene coverage, additional GPU backends and all cutscene compositions remain acceptance work.

The identified high-FPS pose rewind is covered by the separate [animation investigation](animation-interpolation.md). Successful sky tests do not establish animation equivalence. A later reporter capture shows a horizontal black sky/horizon band during some pirate-map replay cameras; that case remains open.

## Full-screen stencil-shadow resolve

A remaining old-frame boundary came from a world effect sharing the flat HUD vertex format. The supported executable's stencil-shadow resolve at `0x4c9100` draws a black, alpha-blended full-screen quad from `0x65ba20`, using FVF `0x44`, four 20-byte vertices and a two-triangle strip. Its vertices use depth zero and RHW one. The generic HUD predicate therefore compressed this world resolve into the centered 4:3 area, clipping the visible shadow darkening at that boundary even when the world and stencil geometry extended farther.

The correction recognizes this exact producer using executable instruction signatures, its fixed source address, enabled stencil testing, the full-screen black vertex pattern and the physical backbuffer target. Only that resolve keeps its full output span. Its inclusive source pixel indices are converted to pixel-edge coordinates before raster scaling, covering the first and last physical pixels too. Ordinary HUD quads retain their proportions, and render-texture draws keep their original coordinates. The source vertex data and stencil state are preserved.

An isolated real-game trace confirmed four producer draws with the expected signature, enabled stencil, FVF `0x44`, 640×480 bounds `(0,0)..(639,479)` and color `0x40000000`. The goal-net setup also references the shadow subsystem through calls at `0x4d0322` and `0x4d037f`.

An asset-free GPU regression exercises the production bridge with these inspected producer signatures and vertices placed in unused padding in its own executable image. With the old bridge, the outer pixel remained white (`ffffff`) while the center received the shadow (`bfbfbf`). The corrected bridge shades both, including the first and last pixels at 1920×1080. An ordinary quad with identical depth/RHW still leaves the side area white. A separate render-target control catches accidental backbuffer edge expansion: an intentionally unguarded implementation fails, while the final correction preserves that path. All runs use an isolated desktop; private evidence is under `local/world-effect-evidence/world-stencil-evidence.json`.

This verifies the specific full-screen stencil resolve and the real producer match. Individual map, goal-net and replay compositions still require broader visual acceptance.

## Localized stencil coverage, 2026-11-17

The asset-free `world_effects_render --masked` case writes two localized stencil
footprints using 3D geometry outside the former centered 4:3 region. Color writes
are disabled during mask construction. The exact inspected screen-space resolve
then darkens both outer patches (`bfbfbf`) while leaving the unmarked center and
upper control white (`ffffff`). Producer vertices and stencil enable/function/
reference/write mask/pass operation and color-write state remain unchanged.

The final fixture fails against the historical uncorrected bridge: both marked
outer samples stay white. It passes against the current production bridge;
all 28 GPU/input/import cases pass in `build/tests/gpu-m_ys8tet/report.json`.
The paired private receipt is
`local/world-effect-evidence/localized-stencil-verification.json`.
Independent review found no actionable issues in this additional coverage.

This strengthens acceptance of the known stencil-composite correction. It does
not prove every real map, shadow producer, net occlusion or replay composition.
No production graphics behavior changes in this verification pass.
