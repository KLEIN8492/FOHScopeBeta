#pragma once
#include "dsp/MtwSpectrum.h"
#include <array>
#include <vector>

namespace fs {

// A − B on the display grid, with A and B lined up in time (usually A = FOH mic, B = console).
// The delay is found by GCC-PHAT cross-correlation every 2 s (median of the last 5 estimates),
// over lags −max…+max: A may arrive after B (mic after console, delay > 0) or before it (the
// patch the other way round, delay < 0). The later signal reaches its spectrum as it comes; the
// earlier one is held back by |delay|, so the trace shows what the PA + room add rather than
// timing smear. Analysis thread only.
class DeltaTrace
{
public:
    static constexpr double kMaxDelaySec = 0.35; // ≈ 120 m of air

    void prepare(double sampleRate);
    void process(const float* a, const float* b, int n); // same clock: pushA + pushB + maybeEstimate
    void pushA(const float* a, int n);
    void pushB(const float* b, int n);
    void maybeEstimate();
    void reset();

    bool valid() const { return hasDelay && specA.hasData() && specB.hasData(); }
    // Signed: > 0 when A arrives after B, < 0 when A arrives first. 0 until a delay is found.
    double delayMs() const { return hasDelay ? 1000.0 * delay / sr : 0.0; }
    // dB, smoothed to 1/6 octave, normalised so the median over 200 Hz – 5 kHz is 0 dB.
    void line(float* out) const;

private:
    void estimateDelay();

    double sr = 48000;
    MtwSpectrum specA, specB;
    // Holds back one stream by `d` samples, reading its ring behind the write position. Reads
    // only what has been written since reset (older slots read 0), so a delay set while the ring
    // is still filling never returns stale samples.
    const float* heldBack(const std::vector<float>& ring, size_t write, size_t filled, const float* x, int n, size_t d);

    std::vector<float> ringA, ringB, held;
    size_t mask = 0, writeA = 0, writeB = 0, filledA = 0, filledB = 0, sinceEstimate = 0;
    int corrOrder = 0, block = 0, maxLag = 0;
    std::unique_ptr<juce::dsp::FFT> corrFft;
    std::vector<juce::dsp::Complex<float>> fa, fb, prod;
    std::vector<int> history;
    int delay = 0;          // samples, signed (see delayMs)
    bool hasDelay = false;
};

} // namespace fs
