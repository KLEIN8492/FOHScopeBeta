# FOHScope — Windows & Linux versions, public releases

Date: 2026-10-11 · Status: draft for review · Extends `2026-10-09-fohscope-design.md` and
`2026-10-09-fohscope-devices-routing-design.md`

## Intent

Release FOHScope to other people on macOS, Windows and Linux, from the public repository
github.com/KLEIN8492/FOHScopeBeta, as free open-source software. Same features and same raw look
on every OS; one code base; the Mac version stays the reference.

Success: a tagged version produces, without manual steps, a GitHub Release with a Mac `.dmg`, a
Windows installer + portable zip, and a Linux AppImage, each built and unit-tested on its OS and
smoke-launched; a newcomer can install and start it from the README alone.

## Licence

FOHScope is released under **AGPLv3** (matches JUCE's AGPLv3 option and the ASIO SDK's GPLv3
option bundled with JUCE 8). Repository gets `LICENSE` (AGPLv3 text) and
`THIRD_PARTY_NOTICES.md` (JUCE — AGPLv3; Steinberg ASIO SDK — GPLv3; JetBrains Mono — SIL OFL 1.1;
Catch2 — BSL-1.0, tests only, not shipped). The About line / README states the licence and links
the source.

## 1. Cross-platform code

### Audio drivers

`DeviceHub` owns one `juce::AudioIODeviceType` per driver the OS offers, obtained from
`juce::AudioDeviceManager::createAudioDeviceTypes()` with these compile flags:

| OS | Driver types (JUCE) | Flags |
|---|---|---|
| macOS | CoreAudio | — |
| Windows | ASIO, Windows Audio (WASAPI shared) | `JUCE_ASIO=1`, `JUCE_WASAPI=1`, `JUCE_DIRECTSOUND=0` |
| Linux | ALSA, JACK | `JUCE_ALSA=1`, `JUCE_JACK=1` |

- **Interface key** (the `PortRef::device` string): on macOS the plain CoreAudio name (existing
  settings stay valid). On Windows/Linux `"<driver>: <name>"`, e.g. `ASIO: Focusrite USB ASIO`,
  `ALSA: US-16x08` — the same hardware through two drivers stays two distinct interfaces.
  `DeviceHub` splits the key back into (type, name) to open it.
- Everything else (input-only open of all channels, shared rate / buffer, background thread,
  watchdog, back-off, states) is unchanged and shared.
- ASIO drivers commonly allow one open instance per process; a second ASIO interface that fails
  to open shows `error: <driver message>` like any other open failure.
- WASAPI is opened in shared mode (input only): fine for analysis, no exclusive takeover of the
  user's device.

### Look, menus, shortcuts

- Font: macOS keeps Menlo. Windows/Linux use **JetBrains Mono** (Regular + Bold, OFL) embedded
  with `juce_add_binary_data`; `theme::mono()` and the LookAndFeel typeface hook select it.
- Shortcuts use `ModifierKeys::commandModifier` (⌘ on macOS, Ctrl elsewhere) — already the case.
  On-screen hints come from one helper: `theme::shortcut(",")` → `CMD+,` / `CTRL+,`.
- Menus: macOS keeps the app menu (`Settings… ⌘,`). Windows/Linux show the same `MenuBarModel`
  as an in-window menu bar (`View › Setup… Ctrl+,`, `Full screen`).
- Microphone permission request: macOS only (`JUCE_MAC`).

### Small portability fixes

- `_USE_MATH_DEFINES` and `NOMINMAX` on MSVC (M_PI, std::min/max).
- Settings directory: macOS `~/Library/Application Support/FOHScope` (unchanged);
  Windows `%APPDATA%\FOHScope`; Linux `~/.config/FOHScope` (`userApplicationDataDirectory`).
- `Tests/` and `Tools/` compile on all three (SignalGen keeps "BlackHole 16ch" as the Mac default;
  the device name is an argument everywhere).
- `--smoke-test` command-line flag: start the app normally (window, analyser, device hub scan),
  run 4 s, quit with exit code 0; any crash or hang fails CI.

## 2. Builds (GitHub Actions)

`.github/workflows/build.yml`, matrix:

| Runner | Compiler | Output |
|---|---|---|
| `macos-14` | AppleClang, universal (arm64 + x86_64), macOS 13+ | `FOHScope-<ver>-mac-universal.dmg` |
| `windows-2022` | MSVC x64 | `FOHScope-<ver>-windows-x64-setup.exe` (Inno Setup) and `FOHScope-<ver>-windows-x64.zip` |
| `ubuntu-22.04` | GCC x86-64 | `FOHScope-<ver>-linux-x86_64.AppImage` (linuxdeploy) |

- Each job: configure (FetchContent JUCE 8.0.15 + Catch2 v3.7.1 — the local `../LiveRack` /
  `../VenueTuner` shortcuts are absent on runners), build Release, run `FOHScopeTests`, run
  `FOHScope --smoke-test` (Linux under `xvfb-run`), package.
- On every push / pull request: build + test + smoke; packages kept as workflow artifacts (7 days).
- On a tag `v*.*.*`: the same, then one job creates the GitHub Release `FOHScope <ver>` with the
  four files and release notes from `CHANGELOG.md`'s section for that version.
- Version: single source in `CMakeLists.txt` (`project(... VERSION x.y.z)`); CI fails if the tag
  and that version differ.
- Ubuntu 22.04 build → AppImage runs on glibc ≥ 2.35 distros (Ubuntu 22.04+, Debian 12+, Fedora 36+).

## 3. Release packaging & docs

- Mac: `.dmg` with the app, Applications link and `READ ME FIRST.txt` (Gatekeeper: System Settings
  › Privacy & Security › Open Anyway). Ad-hoc signed, not notarised.
- Windows: installer (Program Files, Start-menu entry, uninstaller, no admin-only components) and
  portable zip; README notes SmartScreen "More info › Run anyway" (unsigned).
- Linux: AppImage; README notes `chmod +x`, and that the user may need to be in the `audio` group
  for ALSA and that JACK/PipeWire-JACK interfaces appear when a JACK server runs.
- README gets: Download (link to Releases), first-launch steps per OS, licence, build-from-source
  per OS. `docs/TESTING-WINDOWS-LINUX.md`: a 10-minute hardware checklist for testers (device list,
  add input, meters, two interfaces at once, unplug/replug, SPL calibration, quit/relaunch keeps
  settings).

## Publishing rule

Nothing is pushed, tagged or released without the owner's explicit go for that action.

## Testing

- Unit tests (existing 30 cases) on all three OSes in CI.
- New unit tests: interface key split/join (`"ASIO: X"` ↔ (ASIO, X); Mac names without prefix;
  names containing `": "`), settings directory per OS (compile-time), shortcut text.
- Smoke launch on all three OSes in CI.
- Manual on this Mac: full app regression (routing, meters, scale, SPL) after the refactor.
- Windows/Linux real-hardware testing: by the owner / testers with the checklist; not possible
  from this Mac.

## Out of scope

Code signing / notarisation, Linux ARM, Windows ARM, auto-update, distro packages (.deb/.rpm/
Flatpak), installers' file associations.
