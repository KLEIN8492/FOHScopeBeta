# Windows & Linux versions + public releases — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** FOHScope builds, tests and packages on macOS, Windows and Linux from one code base, and tagged versions publish a GitHub Release on KLEIN8492/FOHScopeBeta.

**Architecture:** The only OS-specific code is isolated: audio driver types (DeviceHub + a pure interface-key module), fonts/menus/shortcut text (Theme, Main), the settings folder, and packaging scripts. GitHub Actions builds a 3-OS matrix, runs unit tests and a headless smoke launch, packages, and on `v*.*.*` tags creates the release.

**Tech Stack:** C++20, JUCE 8.0.15 (CoreAudio / ASIO / WASAPI / ALSA / JACK), Catch2 3.7.1, CMake ≥ 3.22, GitHub Actions (macos-14, windows-2022, ubuntu-22.04), Inno Setup 6, linuxdeploy (AppImage), hdiutil.

**Spec:** `docs/superpowers/specs/2026-10-11-fohscope-cross-platform-release-design.md`

## Global Constraints

- Licence AGPLv3; notices for JUCE (AGPLv3), ASIO SDK (GPLv3), JetBrains Mono (OFL 1.1).
- macOS behaviour, settings and interface names unchanged (CoreAudio keys stay unprefixed).
- Windows drivers: ASIO + WASAPI shared (`JUCE_ASIO=1 JUCE_WASAPI=1 JUCE_DIRECTSOUND=0`); Linux: ALSA + JACK (`JUCE_ALSA=1 JUCE_JACK=1`).
- Interface key on Windows/Linux: `"<driver type name>: <device name>"`.
- Settings dir: macOS `~/Library/Application Support/FOHScope`, Windows `%APPDATA%\FOHScope`, Linux `~/.config/FOHScope`.
- Release files: `FOHScope-<ver>-mac-universal.dmg`, `FOHScope-<ver>-windows-x64-setup.exe`, `FOHScope-<ver>-windows-x64.zip`, `FOHScope-<ver>-linux-x86_64.AppImage`.
- Version single source: `project(FOHScope VERSION x.y.z)`; tag must equal `v` + that version.
- No push, tag or release without the owner's explicit go for that action.
- No audio-thread locks/allocations; house style for any UI text.

## Review Focus

1. Windows device names that themselves contain `": "` or non-ASCII (e.g. `Microphone (Realtek(R) Audio)`, `ASIO: Focusrite: 2i2`) — the key must split only after the driver prefix and round-trip exactly. → Task 1 test.
2. Mac settings saved by earlier versions (unprefixed CoreAudio names) must keep resolving to the same interface after the multi-driver change. → Task 1 test.
3. The same hardware listed under two drivers (ASIO and WASAPI) — two distinct interfaces, both selectable, wanted independently. → Task 1 test.
4. A machine with no audio devices at all (headless Linux CI, a VM): the app must start, list no interfaces and run without crashing. → Task 4 smoke test (CI Linux has no ALSA card).
5. First launch on a fresh Windows/Linux account: the settings folder does not exist yet; saving must create it and a second launch must read it back. → Task 2 test (`Settings::saveTo` / `loadFrom` on a temp dir).

---

### Task 1: Interface keys + multi-driver DeviceHub

**Files:** Create `Source/core/InterfaceKey.h/.cpp`; modify `Source/core/DeviceHub.h/.cpp`; test `Tests/test_interface_key.cpp`.

**Produces:**
```cpp
struct InterfaceKey { juce::String type, name; };
juce::String makeInterfaceKey(const juce::String& type, const juce::String& name); // "CoreAudio" → name; else "type: name"
InterfaceKey splitInterfaceKey(const juce::String& key, const juce::StringArray& knownTypes);
// DeviceHub: owns juce::OwnedArray<juce::AudioIODeviceType> from AudioDeviceManager::createAudioDeviceTypes();
// catalogue entries are keyed by makeInterfaceKey(type->getTypeName(), deviceName); openOne() splits the key.
```
- [ ] Tests: `makeInterfaceKey("CoreAudio","BlackHole 16ch") == "BlackHole 16ch"`; `makeInterfaceKey("ASIO","Focusrite USB ASIO") == "ASIO: Focusrite USB ASIO"`; split of `"ASIO: Focusrite: 2i2"` with types {ASIO, Windows Audio} → {ASIO, "Focusrite: 2i2"}; split of `"Windows Audio: Microphone (Realtek(R) Audio)"` → {Windows Audio, "Microphone (Realtek(R) Audio)"}; split of `"BlackHole 16ch"` with types {CoreAudio} → {CoreAudio, "BlackHole 16ch"}; split of unknown prefix `"Foo: Bar"` with {CoreAudio} → {CoreAudio, "Foo: Bar"}; `wantedDevices` with inputs on `"ASIO: X"` and `"Windows Audio: X"` → both in the set.
- [ ] Implement; DeviceHub rescan iterates every type (`scanForDevices`, `getDeviceNames(true)`), probes channel names, opens with `type->createDevice({}, name)`. macOS build + all tests pass; manual Mac regression (routing page lists the same names, existing inputs live).
- [ ] Commit.

### Task 2: Portability (build flags, settings folder, menus, shortcut text, permissions)

**Files:** `CMakeLists.txt`; `Source/core/Settings.h/.cpp`; `Source/ui/Theme.h`; `Source/ui/MainComponent.cpp`; `Source/App/Main.cpp`; test `Tests/test_settings.cpp`, `Tests/test_basics.cpp`.

**Produces:**
```cpp
static juce::File Settings::directory();                 // per-OS folder (Global Constraints)
static Settings Settings::loadFrom(const juce::File& xmlFile); bool Settings::saveTo(const juce::File& xmlFile) const; // load()/save() delegate
juce::String theme::shortcut(const juce::String& key);   // "CMD+," on macOS, "CTRL+," elsewhere
```
- [ ] Tests: `saveTo(temp/"new"/"settings.xml")` creates the missing folder and `loadFrom` reads the same inputs back; `theme::shortcut(",")` equals `"CMD+,"` under `JUCE_MAC` else `"CTRL+,"`; `Settings::directory().getFileName() == "FOHScope"`.
- [ ] CMake: driver flags per OS; MSVC `_USE_MATH_DEFINES NOMINMAX /utf-8 /bigobj`; Linux `find_package(PkgConfig)` + ALSA/JACK/X11/freetype/fontconfig via JUCE's defaults; `JUCE_MAC`-only `MICROPHONE_PERMISSION_*` unaffected. Main.cpp: `setMacMainMenu` on macOS, `setMenuBar(this)` elsewhere; status line and tooltips use `theme::shortcut`. `RuntimePermissions::request` only `#if JUCE_MAC`, else opening allowed immediately.
- [ ] macOS build + tests pass. Commit.

### Task 3: Embedded font for Windows / Linux

**Files:** `Resources/fonts/JetBrainsMono-Regular.ttf`, `Resources/fonts/JetBrainsMono-Bold.ttf`, `Resources/fonts/OFL.txt` (from the official JetBrains Mono release, v2.304; owner approves the download first); `CMakeLists.txt` (`juce_add_binary_data(fs_fonts …)`); `Source/ui/Theme.h`.
- [ ] `theme::mono()` returns Menlo on macOS, else a `juce::Typeface` created from the embedded data (cached); LookAndFeel default-sans hook maps to the same. Test: on non-mac `theme::mono(11).getTypefacePtr() != nullptr` (compiled in CI); macOS unchanged.
- [ ] Commit.

### Task 4: `--smoke-test`

**Files:** `Source/App/Main.cpp`.
- [ ] `initialise(cmdline)`: if it contains `--smoke-test`, start normally, then `juce::Timer::callAfterDelay(4000, quit)`; exit code 0. Manual on macOS: `FOHScope.app/Contents/MacOS/FOHScope --smoke-test; echo $?` → `0` in ≈ 4 s, window shown.
- [ ] Commit.

### Task 5: Packaging scripts

**Files:** `scripts/version.cmake` (prints the project version), `scripts/package-mac.sh`, `packaging/windows/FOHScope.iss`, `scripts/package-windows.ps1`, `scripts/package-linux.sh`, `packaging/linux/fohscope.desktop`, `packaging/READ ME FIRST (Mac).txt`.
- [ ] Mac script: universal build dir → `dist/FOHScope-<ver>-mac-universal.dmg` (app + Applications link + READ ME FIRST, `hdiutil create -format UDZO`), ad-hoc `codesign --force --deep -s -`. Run locally: dmg mounts, app launches, `lipo -archs` shows `x86_64 arm64`.
- [ ] Windows: Inno Setup script (per-user install `{localappdata}\Programs\FOHScope`, Start-menu + uninstaller, icon from `Resources/AppIcon.ico` generated by `make_icon.py`), zip of the exe. Linux: linuxdeploy with the desktop file + 256 px icon → AppImage. (Both verified in CI, Task 6.)
- [ ] Commit.

### Task 6: GitHub Actions

**Files:** `.github/workflows/build.yml`.
- [ ] Matrix (macos-14 universal, windows-2022 MSVC x64, ubuntu-22.04): checkout → (Linux: apt `libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libgl1-mesa-dev xvfb`) → configure Release → build `FOHScope FOHScopeTests` → run tests → smoke (`xvfb-run -a` on Linux) → package → upload artifact. Tag job: check tag == version, `gh release create v<ver> <files> --notes-file <changelog section>`.
- [ ] Validate YAML locally (`python3 -c "import yaml; yaml.safe_load(open(...))"`) and `actionlint` if available. Commit (not pushed).

### Task 7: Licence & docs

**Files:** `LICENSE` (AGPLv3 full text), `THIRD_PARTY_NOTICES.md`, `CHANGELOG.md` (`## 0.2.0` — first public build), `README.md` (Download, first launch per OS, build per OS, licence), `docs/TESTING-WINDOWS-LINUX.md` (10-minute hardware checklist); bump `project(... VERSION 0.2.0)`.
- [ ] Commit.

### Task 8: Publish (each step only on the owner's explicit go)

- [ ] Push `main` → watch the three CI jobs; fix until green.
- [ ] Tag `v0.2.0` and push the tag → release job attaches the four files.
- [ ] Download each file from the release page and check names / sizes; Mac dmg launch check here.
