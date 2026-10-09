# Multi-interface DeviceHub + input-first routing — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Any number of CoreAudio interfaces at once (LiveRack's model), inputs addressed by {device, channel}, and an input-first routing page that starts empty.

**Architecture:** `DeviceHub` opens each wanted interface on its own (input-only, all channels, shared rate/buffer) on a background thread and starts it on a per-interface `DeviceSink` owned by the `Analyser`. The analysis thread drains each sink independently. The UI asks the hub for its catalogue/states and the sinks for meters.

**Tech Stack:** C++20, JUCE 8.0.15, Catch2 3.7.1.

**Spec:** `docs/superpowers/specs/2026-10-09-fohscope-devices-routing-design.md` (extends `2026-10-09-fohscope-design.md`)

**Execution note:** executed inline in the same session; steps give interfaces and exact test assertions, code lands in each task's commit.

## Global Constraints

- No allocation, locks or logging on audio threads (sink callbacks).
- Device open/close never on the message thread.
- `PortRef` encoding identical to LiveRack (`URL-escaped name#channel`).
- InputList: 0…16 inputs.
- No resampling: an interface that does not run at the shared rate is an `error`.
- House style (Theme.h) for all new UI.

---

### Task 1: PortRef, InputList v2, Settings v2 + migration

**Files:** Create `Source/core/PortRef.h/.cpp`; modify `Source/core/Inputs.h`, `Source/core/Settings.h/.cpp`; tests `Tests/test_ports.cpp`, `Tests/test_settings.cpp`, `Tests/test_analyser.cpp` (InputList cases).

**Produces:**
```cpp
struct PortRef { juce::String device; int channel = 0; bool operator==, !=, <; bool valid() const { return device.isNotEmpty(); } };
juce::String encodePort(const PortRef&);  std::optional<PortRef> decodePort(const juce::String&);
struct Input { int id; juce::String name; PortRef port; juce::Colour colour; bool muted, splMic; };
int InputList::add(const PortRef& port);           // -1 when full; first input becomes SPL mic + focus
bool InputList::remove(int id);                    // may go to 0; focus → first or 0
struct Settings { …; double sampleRate = 48000; int bufferSize = 256; juce::String lastDevice; } // deviceState removed
static Settings Settings::fromXml(const juce::XmlElement&); // migrates v1
```
- [ ] Tests: encode/decode round-trip for `"Dante Virtual Soundcard"#3`, names with `# % ; é`, reject `"x#"`, `"#3"`, `"x#3a"`. InputList: add 2, remove both → empty, focusedId == 0, deltas −1; add(port) stores port. Settings v2 round-trip (ports, rate 96000, buffer 128, lastDevice). Migration: v1 XML with `<Input channel="2"/>` and `<DEVICESETUP audioInputDeviceName="BlackHole 16ch" audioDeviceRate="44100" audioDeviceBufferSize="512"/>` → port {"BlackHole 16ch", 2}, rate 44100, buffer 512; v1 without DEVICESETUP → port device = "" (unassigned).
- [ ] Implement, pass, commit.

### Task 2: DeltaTrace independent A / B

**Produces:** `void pushA(const float*, int); void pushB(const float*, int); void maybeEstimate();` (`process(a,b,n)` = pushA + pushB + maybeEstimate).
- [ ] Test: B pink, A = B delayed 20 ms × 0.5; push A in 480-sample chunks and B in 1024-sample chunks (each up to the same time t before every maybeEstimate call at ~10 ms steps) → after 6 s `delayMs()` = 20 ± 0.1. Existing delta tests still pass.
- [ ] Implement, pass, commit.

### Task 3: DeviceSink + multi-device Analyser

**Produces:**
```cpp
class DeviceSink : public juce::AudioIODeviceCallback {  // one per interface
  juce::String name() const; double sampleRate() const; int numChannels() const; bool isRunning() const;
  uint64_t heartbeat() const;            // callbacks so far (watchdog)
  float takePeak(int ch);                // linear, held until read
  void prepareForTest(double sr, int channels); void pushTestBlock(const float* const*, int nCh, int n);
};
std::shared_ptr<DeviceSink> Analyser::sink(const juce::String& device);   // create on demand
std::shared_ptr<DeviceSink> Analyser::findSink(const juce::String&) const;
TraceFrame::live = its sink is running and port.channel < numChannels
Frame: sampleRate/deviceChannels removed
```
- [ ] Tests: two sinks "A" (48 k, 2 ch) and "B" (48 k, 1 ch); input 1 on A#0 (1 kHz), input 2 on B#0 (4 kHz) → each trace peaks at its own tone; stop pushing B → input 1 keeps updating (seq advances, line still valid); cross-device delta A#0 vs B#0 with 20 ms offset → 20 ± 0.1 ms; meters per sink; input on an unknown device → trace not live, no crash.
- [ ] Implement (drain per sink, per-input DSP prepared at its sink's rate, publish on samples of the fastest sink), pass, commit.

### Task 4: DeviceHub

**Files:** `Source/core/DeviceHub.h/.cpp`, `Tests/test_hub.cpp`.

**Produces:**
```cpp
std::set<juce::String> wantedDevices(const InputList&, bool routingVisible, const juce::StringArray& available);
class DeviceHub {
  enum class State { closed, opening, open, error, missing };
  struct Info { juce::String name; juce::StringArray inputs; State state; juce::String error; int restarts = 0; };
  DeviceHub(Analyser&); ~DeviceHub();
  std::vector<Info> snapshot() const;  const Info* (by copy) find(name)
  void setFormat(double rate, int buffer); void setWanted(std::set<juce::String>); void setOpeningAllowed(bool);
  std::function<void()> onChanged;      // message thread
};
```
Worker thread: rescan on start and on device-list change; reconcile (close unwanted, open wanted with back-off 1,2,4…30 s); watchdog every 250 ms (heartbeat stalled 2 s → reopen, restarts++); rate check (`error: runs at X kHz`).
- [ ] Tests: `wantedDevices` = devices of non-muted inputs with a valid port ∪ (routingVisible ? all available : ∅); muted input's device not wanted; device missing from `available` still wanted (shows `missing`).
- [ ] Implement, pass, commit.

### Task 5: UI

Main window uses the hub (no AudioDeviceManager); status line per wanted interface; banner when a wanted interface is missing/error; empty state "NO INPUTS · CMD+, TO ADD". Setup › Audio = AudioPage (rate, buffer, interface list with state/channels/restarts). Setup › Inputs & Routing = input rows (swatch, name, interface combo, channel + meter, SPL/SHOWN/X) + channel strip for the focused row (channel buttons with segmented meters) + `+ ADD INPUT` + delta A/B; hub `routingVisible` while shown. Chip right-click: Channel ▸ interface ▸ channel.
- [ ] Build; manual: BlackHole + MacBook mic + Dante simultaneously, window responsive during Dante open, unplug/replug.
- [ ] Commit.

### Task 6: README + memory

- [ ] README sections Routing / Audio updated. Commit.
