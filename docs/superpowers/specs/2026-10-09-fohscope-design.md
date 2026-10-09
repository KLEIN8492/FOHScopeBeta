# FOHScope — design

Date: 2026-10-09 · Status: approved

## Purpose

A standalone, native macOS real-time spectrum analyser for front-of-house use during a show:
RTA + high-resolution spectrum, spectrograph, feedback/peak finder, calibrated SPL with
noise-limit logging, and an A − B delta trace (e.g. FOH mic − console). Light enough to run
for an entire show on a second screen; it never touches the audio it reads.

## Stack

C++20 / JUCE 8.0.15 (same version as LiveRack), CMake, Catch2 tests. One process, one window
plus a Settings window (⌘,). Spectrograph drawn from an image updated column-by-column.
Reused from VenueTuner (copied, namespace `fs`): `SplMeter`, `Biquad`, `MicCal`-style
calibration offset, `LevelLogger`, `SpscFifo`, `TripleBuffer`, GCC-PHAT-style delay finding.

## Architecture

```
Core Audio device → audio callback (RT): copy all device input channels into one SpscFifo per
                    device channel. No allocation, no locks, nothing else.
Analysis thread (~10 ms tick):
  per input:   MtwSpectrum (hi-res line + fractional-octave bands, averaging, peak hold)
               PeakFinder  (sustained narrow peaks)
  SPL input:   SplMeter + calibration offset → LevelLogger (1 row/s, daily CSV, 183 days)
  Delta:       DeltaTrace(A, B): delay-compensated A − B
  Focused:     spectrograph column
  → TripleBuffer<Frame> (latest snapshot)
UI thread (60 Hz timer): Spectrum view, Spectrograph, Peaks list, SPL panel.
```

The input list is owned by the message thread; edits produce a new `InputConfig` snapshot that
the analysis thread picks up atomically. The audio callback never sees inputs — only device
channels — so adding/removing inputs never touches the RT thread.

## Components

| Unit | Job |
|---|---|
| `InputConfig` | List of inputs: id, name, device channel, colour, muted, `isSplMic`. Min 1, max 16. |
| `Palette` | Next colour = previous hue + 137.5° (golden angle), fixed saturation/brightness for dark UI. |
| `MtwSpectrum` | Multi-time-window FFT: 3 Hann FFTs (32768 / 8192 / 2048 at 48 kHz, doubled at 96 kHz so the time windows stay ≈ 680 / 170 / 43 ms; hop = N/4), each covering its own range (< 250 Hz, 250 Hz–1.5 kHz, > 1.5 kHz). The line is a fixed 1/48-octave grid from 20 Hz (480 points) showing power *density per 1/48 octave* (integration width at least 2 FFT bins, rescaled to 1/48 oct), so pink noise reads flat across window crossovers. Fractional-octave bands are integrated straight from the FFT bins (exact power for tones and noise). Averaging Fast (τ 125 ms), Slow (1 s), Infinite (power mean since reset). Peak hold. Fractional-octave bands (1/1, 1/3, 1/6, 1/12, 1/24) integrated from the grid. Levels in dBFS, AES17 convention: a full-scale sine reads 0 dBFS (so a sine of amplitude 0.1 reads −20 dBFS); band levels are power sums. |
| `PeakFinder` | On the Fast-averaged grid of each input: local maxima ≥ `threshold` dB (default 8) above a 1-octave median-smoothed baseline, narrower than 1/6 oct, persisting ≥ `holdSec` (default 1.5 s) within ±1/24 oct. Reports Hz (parabolic-interpolated), note + cents (A4 = 440), prominence, age. |
| `DeltaTrace` | Finds the delay of B→A by cross-correlation of 1 s blocks (re-estimated every 2 s, median of last 5), delays B accordingly, then per-grid-point Slow-averaged dB(A) − dB(B), smoothed 1/6 oct, shown ±12 dB, normalised to 0 dB median over 200 Hz–5 kHz. |
| `SplMeter` | VenueTuner's: Z/A/C, Fast/Slow/Impulse, Leq, LCpeak, sliding Leq 1 s–15 min. |
| `Calibration` | dB SPL = dBFS + offset. Offset from calibrator (94/114 dB) capture or typed in. Default 120 (uncalibrated, marked "est."). |
| `LevelLogger` | VenueTuner's daily CSV. |
| `Frame` | Snapshot for UI: per input grid (avg + hold), bands, peaks; delta; SPL reading; spectrograph column; sample rate; timestamp. |

## UI

Dark theme. Toolbar: one chip per input (colour dot + name; click = focus, right-click =
rename / colour / channel / mute / SPL mic / remove), ＋ add, averaging FAST/SLOW/INF, band
resolution, Freeze, Capture.

Main area: Spectrum (log 20 Hz–20 kHz, dBFS for every input — SPL lives in the SPL panel); hi-res
line per input; RTA bars for the focused input; peak-hold dotted; captured traces (≤ 6) dashed
grey; feedback markers ▲ on the curve; optional Delta strip (±12 dB). Below, the Spectrograph
of the focused input sharing the x axis (10/30/60 s). Right column: SPL panel (LAeq 15′,
LCeq 15′, LA Fast, LCpeak, limit colouring amber within 3 dB, red above) and Peaks list.

Hover: crosshair with Hz, note, level of each trace. Click pins a cursor.

Keys: Space freeze · C capture · X clear captures · 1–9 focus input · A cycle averaging ·
B toggle bars · D toggle delta · R reset averaging/Leq-hold · F fullscreen · ⌘, settings.

Settings (⌘,): device type/device, sample rate, buffer; inputs table (name, channel, colour,
SPL mic); delta A/B; calibration (calibrator 94/114 or offset); SPL limits (LAeq 15′, LCeq 15′);
peak threshold/hold; spectrograph history and range. Persisted to
`~/Library/Application Support/FOHScope/settings.xml`; logs in `.../FOHScope/levels/`.

## Error handling

- Device disappears: banner "Audio device lost — reconnecting", retries every 2 s, keeps last
  frame visible; inputs keep their channel numbers.
- Input channel beyond the device's count: chip greyed "no ch N".
- Log write failure: SPL panel shows "log ✕" with reason; analysis continues.
- Mic permission denied: banner with instructions.

## Testing

Catch2 unit tests with synthetic signals: 1 kHz sine at −20 dBFS reads −20 ±0.1 dB in its 1/3-oct band; pink noise flat ±0.5 dB on 1/3-oct bands 31 Hz–16 kHz
and ±1 dB on the 1/48 line 40 Hz–16 kHz (incl. across window crossovers);
PeakFinder flags a growing 2.5 kHz tone in pink noise and ignores pink noise / a steady kick;
DeltaTrace recovers a 30 ms delay and a +6 dB shelf; Palette hues never within 20° for the
first 16; SplMeter weightings vs IEC 61672 at 31.5 Hz/1 kHz/8 kHz. Manual: Smaart vs FOHScope
on BlackHole within 0.5 dB per band; 3 h soak with 8 inputs. CPU target < 5 % of one core
with 8 inputs at 48 kHz.

## Out of scope (v1)

Transfer function / phase (that is VenueTuner), network/iPad view, recording audio, AU/VST plugin.
