# Changelog

## 0.3.1

- **SPL warm-up:** LAeq 15 min, LCeq 15 min and LAeq 1 min show dimmed with "WARMING 2:30 / 15:00"
  until their window is full; limit colours stay on.
- **Level log:** the 15-minute columns stay empty until the 15-minute window is full, so no
  15-minute figure is built from a few seconds.

## 0.3.0

- **Scrollable spectrograph:** keeps the last 15 minutes of the focused input. Scroll (wheel /
  trackpad) to go back; the view parks on that moment while recording continues, time labels show
  how long ago (-2:35), LIVE or End returns to live. Freeze parks it too. Hover reads frequency,
  note, level and age. Colour range and unit changes redraw the whole history.
- **Delta trace works in both directions:** A after B (mic after console) or A before B (patched
  the other way round); the delay is shown with its sign.
- **Delta only when both interfaces run at the same rate,** otherwise it stays off instead of
  comparing mismatched samples.
- **Steadier display:** the interface reads its own copy of each analysis frame, so a frame can
  no longer change while it is being drawn.
- **RTA resolution:** an unsupported bands-per-octave value snaps to the nearest supported one
  instead of silently showing 1/3 octave.
- **SPL:** each sliding Leq (1 s … 15 min) now reports how many seconds it has actually averaged
  (groundwork for a "warming up" display).

## 0.2.0

First public build — macOS, Windows and Linux.

- Real-time multi-time-window spectrum and RTA (1/1–1/24 octave), spectrograph, feedback / peak
  finder, calibrated SPL (LA Fast, LAeq / LCeq 15 min with limits, daily CSV log), delay-compensated
  A − B delta trace.
- Any audio interface, several at once, one input per row (interface + channel) with live meters.
  macOS: CoreAudio. Windows: ASIO and Windows Audio (WASAPI). Linux: ALSA and JACK.
- Adjustable dB scale (wheel, pinch, drag, Z to fit, AUTO), spectrograph in dB SPL for the SPL mic.
- Not code-signed yet: see the README for the first-launch step on each OS.
