# FOHScope on Windows / Linux — 10-minute hardware check

For whoever runs a new build on real hardware first. Note the OS, interface(s), driver and FOHScope
version; anything that fails, open an issue on github.com/KLEIN8492/FOHScopeBeta with that info.

1. **Install & start.** Windows: installer, SmartScreen "Run anyway". Linux: `chmod +x`, run the
   AppImage. The window appears within a few seconds, status line shows the sample rate.
2. **Interfaces.** CTRL+, › Audio: your interface is listed with its driver
   (`ASIO: …`, `Windows Audio: …`, `ALSA: …`, `JACK: …`) and the right number of inputs.
3. **Add an input.** Inputs & Routing › `+ ADD INPUT`, pick the interface, click a channel: its
   meter in the strip moves with signal; the state becomes OPEN.
4. **Analysis.** Back in the main window: the trace moves, RTA bars follow, the spectrograph scrolls,
   Z fits the dB scale.
5. **Two interfaces at once.** Add a second input on another interface (or the same hardware
   through the other driver): both traces live together.
6. **Unplug / replug** a USB interface: status line turns red ("missing"), traces stop; after
   replugging it reopens by itself within ~30 s.
7. **SPL.** Mark an input SPL, Setup › Measurement › calibrate (94 dB calibrator or "room reads now"):
   LA Fast reads sensibly; the log folder fills (`%APPDATA%\FOHScope\levels` / `~/.config/FOHScope/levels`).
8. **Restart.** Quit, start again: inputs, colours, routing and scale are restored.
9. **ASIO only:** sample rate / buffer changes in Setup › Audio reopen the interface without a crash.
10. **CPU.** With 4 inputs live, Task Manager / `top` shows FOHScope under ~25 % of one core.
