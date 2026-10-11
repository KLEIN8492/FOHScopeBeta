# Changelog

## 0.2.0

First public build — macOS, Windows and Linux.

- Real-time multi-time-window spectrum and RTA (1/1–1/24 octave), spectrograph, feedback / peak
  finder, calibrated SPL (LA Fast, LAeq / LCeq 15 min with limits, daily CSV log), delay-compensated
  A − B delta trace.
- Any audio interface, several at once, one input per row (interface + channel) with live meters.
  macOS: CoreAudio. Windows: ASIO and Windows Audio (WASAPI). Linux: ALSA and JACK.
- Adjustable dB scale (wheel, pinch, drag, Z to fit, AUTO), spectrograph in dB SPL for the SPL mic.
- Not code-signed yet: see the README for the first-launch step on each OS.
