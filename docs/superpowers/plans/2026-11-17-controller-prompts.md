# Controller Prompt Families Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Ship glossy, textured Xbox 360 prompts and matching PS4/Switch Pro/Steam Deck schemes in game menus, internal settings and launcher controls, with automatic family detection and manual preference.

**Architecture:** Preserve SDL positional action semantics and independent four-player slots. Detect device family through SDL_GetGamepadType (name fallback for raw devices); publish the active family through reserved InputMailbox bits 8â€“10 without changing page size. A shared controller-prompt atlas and metadata provide native and launcher glyphs. Auto uses the last active/selected controller; manual style overrides display only. Nintendo south/east remain B/A, so displayed hints match actual actions. Existing XboxPrompts on/off preferences migrate intact.

**Assets:** Xelu's original CC0 pack from thoseawesomeguys.com/prompts, downloaded SHA-256 2848fd4259437a6f594f81ff8d8d7a1f922397c7e9da163b2535aa82ae1df037. Preserve selected PNGs/readme, source manifest and credit. Xbox 360 face assets provide beveled colored material; apply a code-rendered gloss highlight if visual acceptance needs more sheen. PS4 colored symbols and Switch labels retain each controller's physical positions.

## Tasks

- [x] Add regression tests for family detection, face-position labels, unknown fallback, manual/Auto preference validation and the mailbox bit contract. Verify failure before implementing family helpers.
- [x] Add selected licensed assets, manifest, native atlas generation and preview. Preserve upstream source PNGs and license; package required files and native resource deterministically.
- [x] Implement SDL type discovery and active-family publishing. Preserve focus gates, activity ownership, four-seat independence and input actions. Test mixed-family hot plug and absent/malformed family metadata through fixtures.
- [x] Add Auto/Xbox360/PS4/SwitchPro/SteamDeck preference to launcher/settings/runtime. Rename Xbox-only UI wording, retain old on/off settings, and show matching controls labels/preview with correct positional names.
- [x] Implement shared native glyph loading and use it for original menu prompt replacement and F1 footer. Add a live family selector; release/recreate cached textures correctly on style/device changes.
- [x] Run focused Python and native tests, GPU prompt fixtures for all families, isolated frozen launcher QA and actual private-game screenshots. Verify fallback when assets/metadata are invalid. No foreground desktop switching or physical-controller claims from virtual fixtures.
- [x] Review, integrate and publish a new verified baseline if release gates pass. Original broader game/network requirements stay active.

## Files

- `assets/controllers/`, `licenses/Xelu-Controller-Prompts.txt`, `THIRD_PARTY.md`: selected upstream glyphs, provenance and license.
- `tools/build_controller_prompts.py`, `native/controller_prompts.h`, `native/branding.rc`, `tools/build_native.ps1`: deterministic atlas/resource and native family/renderer helpers.
- `opensoccermania/controller_prompts.py`, `controllers.py`, `video.py`, `runtime.py`, `launcher.py`: names, SDL detection, protocol, persisted preference and preview.
- `native/input_page.h`, `menu_background.h`, `settings_menu.h`, `d3d8_proxy.cpp`: stable family read and menu/settings integration.
- `tests/test_controller_prompts.py`, native/GPU fixtures, `tools/test_native.ps1`, `tools/test_gpu.py`: meaningful regression coverage.

## Acceptance limits

A family selector and glyphs are not proof that a physical DualShock/Switch device has been exercised. Verify SDL mapping contracts and fixture behavior; report hardware-dependent checks honestly. Do not silently swap Nintendo gameplay actions, invent an online-ready claim, or alter owner assets/saves.

## Execution evidence

- Inline execution; final read-only whole-branch review found a packaging
  allowlist omission. Added an exact-path regression (observed failure), fixed
  both packagers, and passed 25 focused controller/source/portable checks.
- Private launcher startup reproduced a pre-controller initialization error;
  using a safe default until discovery exists restores startup and close. All
  four image/label previews, Auto and persisted manual preference pass.
- Native GPU variants use the controller-present flag and reserved family bits;
  their rendered footer glyph-only regions differ across all four families.
  All 16 atlas faces survive texture upload exactly.
- Real owner game: 16 private commands verify F1 manual cycling, persisted values,
  original-menu prompts and clean shutdown. Frozen smoke and a real standard
  token with NTFS write denial pass. No physical-hardware claim is inferred.
- Ruling: existing user authorization covers integration/publication of successive
  baselines; proceed through final archive checks without another approval menu.
  Broader game/network requirements stay active.

- Published pre.8 from `899fd96a84d2efe4528ecc30621f22760b8d9333` after exact-DLL checks, CI, extracted-source tests and extracted frozen smoke. Source/portable inventories and all four public download hashes match.
