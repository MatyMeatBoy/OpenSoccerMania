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
