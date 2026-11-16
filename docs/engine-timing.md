# Engine timing investigation

The preferred simulation clock is the fastest verified original engine path. Rendering must remain independent of simulation speed.

## Evidence from the supplied PC executables

Both valid executables contain the same rate-selection routine at VA `0x416760`. Its branches select 30, 37.5 or 50. The 50 branch includes a vertex-shader capability test. Consequently, these values cannot currently be classified as a PAL versus NTSC distinction. A live run reported the configured engine value as 50.

The executable also contains `pc_cd_pal` and `pc_cd_ntsc` asset paths, selected by English versus American language configuration. This alone does not establish different simulation clocks. The supplied Electronic Arts dump has a zero-filled executable, so it cannot establish a separate American executable clock.

## Experimental bridge

For recognized executable hashes and matching function signatures, the bridge instruments simulation steps and separates the render wait from the original scheduler's simulation-rate argument. It logs `configured_logic_hz`, observed step counts and elapsed time in `opensoccermania-engine.log`. A render target of 240 FPS is a limit, not a performance guarantee.

Startup, gameplay and clean shutdown passed with this bridge. Initial navigation samples stayed in menus and correctly recorded zero simulation steps. Subsequent active-game samples observed approximately 50 simulation steps per second with rendering around 140 FPS and VSync enabled. English and American both retained a configured 50 Hz rate; American selected `Software\LEGO Media\Soccer Mania` and the Soccer title assets. This is a comparison of region modes in the supplied executable, not proof about an independently dumped American executable.

The fastest currently verified original selection is therefore **50 Hz**. No 60 or 59.94 Hz original simulation path has been established. Full match-timer, audio and long-session equivalence across render caps remain acceptance work. Local machine-readable samples and native logs are kept under ignored `local/timing-evidence/`.
