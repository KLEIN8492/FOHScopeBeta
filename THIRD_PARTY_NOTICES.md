# Third-party software in FOHScope

FOHScope is free software: AGPLv3 (see `LICENSE`). Source: https://github.com/KLEIN8492/FOHScopeBeta

| Component | Used for | Licence |
|---|---|---|
| [JUCE](https://juce.com) 8.0.15 | audio drivers, UI, DSP helpers | AGPLv3 (JUCE is dual-licensed AGPLv3 / commercial; FOHScope uses AGPLv3) |
| Steinberg ASIO SDK headers (bundled with JUCE) | ASIO interfaces on Windows | GPLv3 (dual-licensed Steinberg ASIO Licence / GPLv3; FOHScope uses GPLv3). ASIO is a trademark and software of Steinberg Media Technologies GmbH. |
| [JetBrains Mono](https://www.jetbrains.com/lp/mono/) | interface font on Windows and Linux | SIL Open Font License 1.1 (`Resources/fonts/OFL.txt`) |
| [Catch2](https://github.com/catchorg/Catch2) 3.7.1 | unit tests only (not shipped) | Boost Software License 1.0 |

The analysis code reuses parts of VenueTuner (same author).
