# FOHScope

Real-time FOH spectrum analyser for **macOS, Windows and Linux**: multi-time-window spectrum + RTA,
spectrograph, feedback finder, calibrated SPL with noise-limit logging, and a delay-compensated
A − B delta trace (e.g. FOH mic − console). 1–16 inputs from any audio interfaces, several at once.

Free software under the **AGPLv3** (`LICENSE`, third-party notices in `THIRD_PARTY_NOTICES.md`).

## Download

Get the latest version from **[Releases](https://github.com/KLEIN8492/FOHScopeBeta/releases)**:

| System | File | First launch (the apps are not code-signed yet) |
|---|---|---|
| macOS 13+ (Apple Silicon & Intel) | `FOHScope-<ver>-mac-universal.dmg` | Drag to Applications, open once, then System Settings › Privacy & Security › **Open Anyway**. Allow microphone access. |
| Windows 10/11 (x64) | `FOHScope-<ver>-windows-x64-setup.exe` (or the portable `.zip`) | SmartScreen: **More info › Run anyway**. No admin rights needed. |
| Linux x86-64 (Ubuntu 22.04+, Debian 12+, Fedora 36+) | `FOHScope-<ver>-linux-x86_64.AppImage` | `chmod +x FOHScope-*.AppImage` then run it. Your user may need to be in the `audio` group; JACK / PipeWire-JACK interfaces appear while a JACK server runs. |

Audio drivers: macOS CoreAudio · Windows ASIO and Windows Audio (WASAPI) · Linux ALSA and JACK.
On Windows and Linux an interface shows with its driver, e.g. `ASIO: Focusrite USB ASIO`.

Settings and SPL logs: macOS `~/Library/Application Support/FOHScope` · Windows
`%APPDATA%\FOHScope` · Linux `~/.config/FOHScope`.

## Build from source

CMake ≥ 3.22 and a C++20 compiler. JUCE 8.0.15 and Catch2 are downloaded on first configure
(or taken from LiveRack / VenueTuner checkouts next to this folder).

```bash
# macOS (Xcode command-line tools)
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release && cmake --build build -j10
scripts/package-mac.sh                     # universal .dmg in dist/

# Windows (Visual Studio 2022)
cmake -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Release
pwsh scripts/package-windows.ps1           # installer + zip in dist/ (needs Inno Setup 6)

# Linux (Debian/Ubuntu packages)
sudo apt install libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev libx11-dev \
  libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libgl1-mesa-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
scripts/package-linux.sh                   # AppImage in dist/ (needs linuxdeploy)
```

Tests: `build/FOHScopeTests` (Windows: `build\Release\FOHScopeTests.exe`), analysis load:
`build/FOHScopeTests "[.bench]"`. Every push is built, tested and smoke-launched on all three
systems by GitHub Actions; a tag `vX.Y.Z` (matching `project(... VERSION)`) publishes a release.

Test without hardware (macOS): `build/FOHScopeSignalGen "BlackHole 16ch"` plays a simulated FOH
mic (ch1: desk delayed 25 ms + shelf + a 2.5 kHz ring every 20 s) and desk L/R pink (ch2/3).
Quit FOHScope from its menu (CMD+Q), not with `kill`: a killed audio client can leave coreaudiod
stuck (`sudo killall coreaudiod` fixes it).

Design docs: `docs/superpowers/specs/`, plans: `docs/superpowers/plans/`.

## Using it

- **Inputs & Routing (CMD+, / CTRL+,)**: starts empty. `+ ADD INPUT`, pick the input's **interface**, then
  click its **channel** in the strip below (every channel with a live meter). Any interface on
  the Mac, several at once (each opened on its own, LiveRack-style; no aggregate device).
  Per row: colour (click swatch), name (click when selected), SPL / SHOWN / X. Delta A/B below.
- **Audio (CMD+, / CTRL+,)**: one sample rate and buffer for every interface (no resampling), and the
  state of each interface (open / opening / error / missing, restarts). Interfaces open in the
  background and reopen on their own after an unplug.
- **Inputs bar**: click a chip to focus it (RTA bars, peak hold, spectrograph); right-click for
  quick changes (Channel = interface > channel); `+` opens routing with a new input.
- **Settings (CMD+, on macOS, CTRL+, on Windows / Linux)**: Inputs & Routing · Audio · SPL calibration (94/114 dB calibrator, or
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
| Z | fit the dB scale to the material (AUTO in the bar keeps following) |
| wheel / cmd+wheel / pinch | pan / zoom the dB scale (or drag the dB labels; double-click them to fit) |
| CMD+, / CTRL+, | setup |
