# Pre.9 probe and validation baseline

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task by task.

**Goal:** Publish the user's requested next baseline with truthful render-cap
configuration and observable, orderly private-probe shutdown.

**Architecture:** Keep game/native behavior unchanged. Correct the existing
Python owner-game probe's configuration and cleanup in place. Use the actual
private desktop and exact child PID for window selection; record WM_CLOSE
delivery and wait for process exit without injecting WM_QUIT. Retain forced
termination as an explicitly failed cleanup result.

**Tech stack:** Python, ctypes/Win32, existing native bridge, PyInstaller and
the repository's checked source/portable ZIP packagers.

- [x] Reproduce `--fps 60` generating RenderFPS=0 in an isolated owner-game copy.
- [x] Pass the requested cap into VideoSettings, remove the unused ProbeFPS
  diagnostic, and record actual configuration plus close-delivery evidence.
- [x] Run the same owner-game control with 60 and 120, checking generated INI,
  native startup log and clean exit; keep logical Hz unchanged.
- [x] Review the focused code change and update release notes/version to pre.9.
- [ ] Preserve pre.8, rebuild/freeze and verify Python/native/GPU checks against
  the exact packaged binaries. Verify extracted source, frozen UI/import and
  standard-user storage before publishing immutable pre.9 assets and hashes.

This release is a diagnostic/validation baseline. Physical hardware, campaign,
all-map shadows/replays and synchronized online gameplay remain separate work.

Ruling: retain the existing dedicated checkout for this bounded release-tool
correction. Private owner-game fixtures and acceptance scripts are rooted here;
there are no concurrent source changes. Keep unrelated private data untouched.

Focused evidence: the old 60 request produces RenderFPS=0; corrected requests
60/120 reach the native startup configuration, retain configured logic 50 Hz,
and exit zero after delivered WM_CLOSE in 0.187/0.203 seconds. Receipts stay in
`local/release-verification/pre9-build/`. This does not establish measured FPS.

Review also found the old advertised 50 limit was unsupported by VideoSettings.
The real CLI help regression fails for that option before correction and passes
after deriving choices from the shared limit list. Final source checks pass
149 Python tests (one privilege skip), 22 native cases and 31 GPU/input/import
cases against the exact frozen bridge. Frozen UI/import/resources and actual
nonadministrator NTFS-denial startup pass, including restored fixture ACLs.
