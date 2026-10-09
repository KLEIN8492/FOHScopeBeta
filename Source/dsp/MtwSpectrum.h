#pragma once
#include "dsp/FreqGrid.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include <vector>

namespace fs {

enum class Averaging { Fast, Slow, Infinite };

struct Band
{
    float fc = 0, db = -200;
};

// Multi-time-window spectrum of one signal. Three Hann FFTs (≈ 680 / 170 / 43 ms at any sample
// rate, hop N/4) each own a frequency range (< 250 Hz, < 1.5 kHz, above), so the lows get
// resolution and the highs get speed. Bin powers are averaged three ways at once (Fast τ 125 ms,
// Slow τ 1 s, Infinite mean) so every consumer can pick its own.
//
// Levels are dB re full-scale RMS (10·log10 of mean square).
//  - line(): power density per 1/48 octave on the shared grid (pink noise reads flat).
//  - bands(): exact power in fractional-octave bands (tones and noise both read true).
// Analysis thread only.
class MtwSpectrum
{
public:
    static constexpr int kWindows = 3;
    static constexpr int kResolutions[] = { 1, 3, 6, 12, 24 };

    void prepare(double sampleRate);
    void process(const float* x, int n);
    void resetAveraging();

    bool hasData() const { return windows[0].frames > 0 && windows[kWindows - 1].frames > 0; }
    void line(Averaging, float* out) const; // grid::kPoints values
    std::vector<Band> bands(Averaging, int bandsPerOctave) const;

    void updateHold(const float* line);
    void hold(float* out) const;

private:
    struct Window
    {
        int N = 0, hop = 0;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> window, ring, buf;
        std::vector<double> fast, slow, inf;
        double aFast = 0, aSlow = 0;
        size_t writePos = 0, sinceFrame = 0;
        long long frames = 0, infFrames = 0;
        double scale = 0; // 2 / (N·Σw²): bin power → mean square per bin
        double df = 0;
        void frame();
    };
    struct Tap
    {
        int k;
        float weight;
    };
    // Sum of weighted bin powers from one window. Precomputed per grid point / band.
    struct Integral
    {
        int window;
        std::vector<Tap> taps;
    };

    Integral integrate(double lo, double hi, double fc) const;
    double evaluate(const Integral&, Averaging) const;
    int windowFor(double hz) const { return hz < 250.0 ? 0 : hz < 1500.0 ? 1 : 2; }

    double sr = 48000;
    std::array<Window, kWindows> windows;
    std::vector<Integral> lineTaps;
    std::array<std::vector<std::pair<float, Integral>>, std::size(kResolutions)> bandTaps;
    std::vector<float> holdDb;
};

} // namespace fs
