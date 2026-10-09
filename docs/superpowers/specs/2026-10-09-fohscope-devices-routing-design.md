# FOHScope — any interface, input-first routing

Date: 2026-10-09 · Status: draft for review · Extends `2026-10-09-fohscope-design.md`

## Goal

Use any audio interface on the Mac, several at once (LiveRack's model), and route with an
input-first page: start empty, add an input, choose its interface, choose its channel while
seeing every channel's level.

## Audio: DeviceHub (LiveRack's model)

- `PortRef { juce::String device; int channel; }` (0-based) identifies a source, as in LiveRack.
  `Input::channel` becomes `Input::port`.
- `DeviceHub` owns every input-capable CoreAudio interface:
  - Catalogue: name + input channel names, refreshed on device-list changes.
  - Each interface is opened on its own (no aggregate, no JUCE in/out combiner), all input
    channels active, no outputs, at one shared sample rate and buffer size (Setup › Audio).
  - Wanted set = interfaces used by any input ∪ (every input interface while the routing page is
    visible). Interfaces outside the wanted set are closed.
  - Opening and closing run on a background thread: a slow driver (Dante ≈ 15 s here) never
    blocks the window. State per interface: `closed | opening | open | error(msg) | missing`.
  - Watchdog: an open interface whose callback stops for 2 s, or that disappears, is reopened
    with back-off 1, 2, 4 … 30 s. Unplug → `missing` → reopened when it comes back.
  - An interface that cannot run the shared rate shows `error: 44.1 kHz only` (no resampling).
- Each open interface has its own callback object feeding the Analyser (per-channel FIFO +
  per-channel peak meter), exactly the current single-device path, times N.

## Analysis

- Per interface: drain the common amount of its used channels (sample-aligned within one
  interface); interfaces never wait on each other, so a stalled one cannot freeze the rest.
- Inputs, SPL mic, spectrograph: unchanged, fed from their port.
- Delta A/B: A and B are pushed independently (`DeltaTrace::pushA / pushB`); each keeps its own
  ring; the GCC-PHAT estimate uses the newest block of each and is re-run every 2 s, which also
  absorbs clock drift between unsynced interfaces (tens of ppm).

## Inputs & Routing page (replaces the matrix)

- Empty by default: "NO INPUTS — + ADD INPUT". `InputList` minimum becomes 0; the main window
  shows "NO INPUTS · CMD+, TO ADD" when there are none.
- One row per input: colour swatch (click: colour) · name (click: rename) · INTERFACE combo (every
  input interface, with its state) · selected channel + its live meter · SPL / SHOWN / X tags.
- Channel picker: under the focused row, a strip of the chosen interface's channels, each a
  button "3 ANALOG 3" with a segmented meter; the selected one is accent-filled; click to patch.
- `+ ADD INPUT`: new row on the last-used interface (else the first), picker open.
- Delta A / B pickers under the list (as now).
- Chips' right-click menu: Channel submenu lists interface ▸ channel.

## Setup › Audio (replaces JUCE's device selector)

Sample rate and buffer size (shared), and a list of interfaces with state, channel count and
reopen count. No device "selection": interfaces are chosen per input.

## Settings format

`<Input port="Dante%20Virtual%20Soundcard#3" …/>` (LiveRack's URL-escaped encoding).
Migration: an input with a bare `channel` gets the device saved in the old `DEVICESETUP`
(input device name), else the system default input. `DEVICESETUP` is replaced by
`<Audio rate="48000" buffer="256"/>`.

## Status line

`FOHSCOPE · 48.0 kHz / 256 · DANTE VIRTUAL SOUNDCARD ok · USB 2i2 opening…` — one item per wanted
interface, red when `error`/`missing`.

## Testing

- PortRef encode/decode round-trip, odd names (spaces, #, %, ;, UTF-8).
- Settings migration from a v1 file (bare channels + DEVICESETUP) and round-trip of v2.
- InputList: 0 inputs allowed; add/remove keep focus/delta consistent.
- Analyser with two test devices: each input fed from its own device; a device that stops
  pushing does not stall the other's traces; delta across devices with independent pushes and
  a 20 ms offset finds 20 ms.
- Wanted-set computation (inputs ∪ routing-page-visible) as a pure function.
- Manual: BlackHole + MacBook mic + Dante at once; unplug/replug a USB interface; Dante slow
  open with the window responsive.

## Out of scope

Resampling between interfaces at different rates, outputs, word-clock / drift correction.
