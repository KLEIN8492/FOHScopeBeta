#pragma once
#include "dsp/MtwSpectrum.h"
#include <array>
#include <vector>

namespace fs {

// A − B on the display grid, with B delayed to line up with A (A = FOH mic, B = console feed).
// The delay is found by GCC-PHAT cross-correlation every 2 s (median of the last 5 estimates),
// so the trace shows what the PA + room add rather than timing smear. Analysis thread only.
class DeltaTrace
{
public:
    static constexpr double kMaxDelaySec = 0.35; // ≈ 120 m of air

    void prepare(double sampleRate);
    void process(const float* a, const float* b, int n);
    void reset();

    bool valid() const { return delay >= 0 && specA.hasData() && specB.hasData(); }
    double delayMs() const { return delay < 0 ? 0.0 : 1000.0 * delay / sr; }
    // dB, smoothed to 1/6 octave, normalised so the median over 200 Hz – 5 kHz is 0 dB.
    void line(float* out) const;

private:
    void estimateDelay();

    double sr = 48000;
    MtwSpectrum specA, specB;
    std::vector<float> ringA, ringB, delayedB;
    size_t mask = 0, writePos = 0, filled = 0, sinceEstimate = 0;
    int corrOrder = 0, block = 0, maxLag = 0;
    std::unique_ptr<juce::dsp::FFT> corrFft;
    std::vector<juce::dsp::Complex<float>> fa, fb, prod;
    std::vector<int> history;
    int delay = -1;
};

} // namespace fs
