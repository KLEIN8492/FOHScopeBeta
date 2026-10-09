#pragma once
#include "dsp/FreqGrid.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>

namespace fs::theme {

// Dark FOH palette: low glare in a dark booth, traces carry the colour.
inline const juce::Colour bg { 0xff0d0f13 }, panel { 0xff151920 }, panelHi { 0xff1d222b }, gridLine { 0xff232932 },
    gridStrong { 0xff2f3743 }, text { 0xffd3dae3 }, dim { 0xff7a8492 }, amber { 0xffffb020 }, red { 0xffff4d4d },
    green { 0xff3ddc84 }, capture { 0xff9aa4b2 };

// Source literals are UTF-8; juce::String(const char*) assumes ASCII, so wrap any non-ASCII text.
inline juce::String utf8(const char* s) { return juce::String::fromUTF8(s); }

// dB re FS RMS (internal) → dBFS as shown (AES17: a full-scale sine reads 0 dBFS).
constexpr float kAes17 = 3.0103f;

inline juce::Font mono(float h) { return juce::Font(juce::FontOptions("Menlo", h, juce::Font::plain)); }
inline juce::Font sans(float h, bool bold = false) { return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain)); }

// Plot insets shared by the spectrum and the spectrograph so their frequency axes line up.
constexpr int kPlotLeft = 46, kPlotRight = 14;

// Log frequency axis 20 Hz – 20 kHz across a pixel span.
struct FreqAxis
{
    float x0 = 0, x1 = 1;
    static constexpr double lo = 20.0, hi = 20000.0;
    float x(double hz) const { return x0 + (x1 - x0) * float(std::log(hz / lo) / std::log(hi / lo)); }
    double hz(float px) const { return lo * std::pow(hi / lo, double((px - x0) / (x1 - x0))); }
};

inline juce::String hzText(double hz)
{
    return hz >= 1000.0 ? juce::String(hz / 1000.0, hz >= 10000.0 ? 1 : 2) + "k" : juce::String(juce::roundToInt(hz));
}

// Spectrograph colour map (black → violet → red → orange → yellow → white), 256 entries.
inline const std::array<juce::PixelARGB, 256>& heatMap()
{
    static const auto lut = [] {
        std::array<juce::PixelARGB, 256> t {};
        const juce::Colour stops[] = { juce::Colour(0xff000000), juce::Colour(0xff1b0c41), juce::Colour(0xff6a176e), juce::Colour(0xffbb3754),
                                       juce::Colour(0xfff1711f), juce::Colour(0xfffbc72a), juce::Colour(0xfffcffa4) };
        constexpr int n = int(std::size(stops)) - 1;
        for (int i = 0; i < 256; ++i) {
            const float p = float(i) / 255.0f * n;
            const int s = std::min(int(p), n - 1);
            t[size_t(i)] = stops[s].interpolatedWith(stops[s + 1], p - float(s)).getPixelARGB();
        }
        return t;
    }();
    return lut;
}

} // namespace fs::theme
