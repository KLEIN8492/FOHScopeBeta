# Third-party software in FOHScope

FOHScope — real-time FOH analyser. Copyright (C) 2026 Thomas Chaudon.
Free software under the GNU Affero General Public License v3 (`LICENSE`).
Source: https://github.com/KLEIN8492/FOHScopeBeta

FOHScope includes the following third-party code. Licence texts are in the JUCE source
(`modules/…`, paths below) and, for the font, in `OFL.txt` shipped next to this file.

| Component | Used for | Licence |
|---|---|---|
| [JUCE](https://juce.com) 8.0.15 | audio drivers, interface, DSP helpers | AGPLv3 (JUCE is dual-licensed AGPLv3 / commercial; FOHScope uses AGPLv3) |
| Steinberg ASIO SDK headers (bundled with JUCE, `juce_audio_devices/native/asio`) | ASIO interfaces — Windows build only | GPLv3 (dual-licensed Steinberg ASIO Licence / GPLv3; FOHScope uses GPLv3). ASIO is a trademark and software of Steinberg Media Technologies GmbH. |
| zlib (`juce_core/zip/zlib`) | compression in JUCE | zlib licence — Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler |
| libpng (`juce_graphics/image_formats/pnglib`) | PNG images (icons) | libpng licence (zlib-style) |
| IJG libjpeg (`juce_graphics/image_formats/jpglib`) | JPEG decoding in JUCE | Independent JPEG Group licence — *this software is based in part on the work of the Independent JPEG Group.* |
| HarfBuzz (`juce_graphics/fonts/harfbuzz`) | text shaping | Old MIT licence |
| SheenBidi (`juce_graphics/unicode/sheenbidi`) | bidirectional text | Apache License 2.0 |
| [JetBrains Mono](https://www.jetbrains.com/lp/mono/) 2.304 | interface font (Windows / Linux; embedded in every build) | SIL Open Font License 1.1 (`OFL.txt`) |
| [Catch2](https://github.com/catchorg/Catch2) 3.7.1 | unit tests only — not shipped | Boost Software License 1.0 |

The analysis code reuses parts of VenueTuner (same author).
