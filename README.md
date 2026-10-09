# FOHScope

Real-time FOH spectrum analyser for macOS: multi-time-window spectrum + RTA, spectrograph,
feedback finder, calibrated SPL with noise-limit logging, and a delay-compensated A − B delta
trace (e.g. FOH mic − console). 1–16 inputs, each with its own colour.

Design: `docs/superpowers/specs/2026-10-09-fohscope-design.md` · Plan: `docs/superpowers/plans/`

## Build & run

Needs Xcode command-line tools and CMake ≥ 3.22. JUCE and Catch2 are taken from the LiveRack /
VenueTuner checkouts next to this folder when present, otherwise downloaded.

```bash
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j10
open build/FOHScope_artefacts/Release/FOHScope.app
build/FOHScopeTests                  # unit tests
build/FOHScopeTests "[.bench]"       # analysis load, 8 inputs
```

Test without hardware: `build/FOHScopeSignalGen "BlackHole 16ch"` plays a simulated FOH mic
(ch1: desk delayed 25 ms + shelf + a 2.5 kHz ring every 20 s) and desk L/R pink (ch2/3).
Quit FOHScope from its menu (⌘Q), not with `kill`: a killed audio client can leave coreaudiod
stuck (`sudo killall coreaudiod` fixes it).

## Using it

- **Routing (⌘, → Inputs & Routing)**: patch matrix. Rows = the device's input channels with live
  meters (find the mic before patching), columns = FOHScope inputs. Click a cell to patch; click a
  column's name to rename, its dot for colour; SPL / Shown / × pills; `+ Add input`. Delta A/B
  pickers under the matrix.
- **Inputs bar**: click a chip to focus it (RTA bars, peak hold, spectrograph); right-click for
  quick changes during a show.
- **Settings (⌘, from anywhere)**: Inputs & Routing · Audio device · SPL calibration (94/114 dB calibrator, or
  "room reads now" reference, or an offset) · LAeq/LCeq 15 min limits · feedback threshold/hold ·
  spectrograph history and colour range · spectrum scale.
- Levels in the spectrum are dBFS (AES17: full-scale sine = 0 dBFS). SPL is in the right panel,
  logged every second to `~/Library/Application Support/FOHScope/levels/YYYY-MM-DD.csv`
  (kept 6 months).

| Key | Action |
|---|---|
| Space | freeze display (logging continues) |
| C / X | capture focused trace as reference / clear captures (max 6) |
| 1–9 | focus input |
| A | cycle averaging Fast / Slow / Inf |
| B / D | RTA bars / delta strip |
| R | reset Inf averaging, peak hold, Leq-since-reset |
| F | full screen |
| ⌘, | settings |
