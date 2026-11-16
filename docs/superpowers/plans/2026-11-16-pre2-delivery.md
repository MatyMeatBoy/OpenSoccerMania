# Prerelease 2 delivery plan

**Goal:** Publish a scoped 0.1.0-pre.2 with reproducible source packaging and a
Windows x64 portable launcher driving the existing x86 game bridge.

**Design:** Keep PyInstaller's one-folder layout beside the launcher executable
so existing resource paths remain valid. Include only project assets, licensed
runtime dependencies and project-built bridges. Original game inputs and the
graphics backend are supplied by the owner. Source archives use committed blobs,
not mutable workspace files. Native world-effect work ships only with evidence.

**Constraints:** English repository/UI. Preserve simulation timing. Do not control
the input desktop or alter the three original dumps. Do not overwrite delivered
release archives. Physical controllers, all-map rendering and hardware VRR remain
unverified unless separately exercised.

- [x] Diagnose CI failure and compare canonical fixture paths.
- [x] Fix non-Hor+ camera presets with projection regression checks.
- [x] Investigate remaining world-effect framing; integrate only proven fixes.
- [x] Add source archive builder with committed-input inventory and exclusion tests.
- [x] Add portable build recipe and a packaged-launcher smoke entry point that
  validates resources, creates the real UI and shuts down without game imports.
- [x] Run Python/native checks, affected GPU regressions and isolated packaged UI.
- [ ] Commit final sources, close/hash archives, validate extracted package,
  publish pre.2 and verify tag, asset digests and prerelease visibility.

The smoke test must use a fresh temporary settings location, preserve existing
preferences, write a machine-readable report, and avoid starting the game.
