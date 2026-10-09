#include "dsp/DeltaTrace.h"
#include <algorithm>
#include <cmath>

namespace fs {

void DeltaTrace::prepare(double sampleRate)
{
    sr = sampleRate;
    maxLag = int(kMaxDelaySec * sr);
    // Correlation block ≥ 2 × the longest lag so even the latest arrival overlaps half of it.
    block = int(juce::nextPowerOfTwo(2 * maxLag));
    corrOrder = int(std::log2(block)) + 1; // zero-padded to 2 × block: linear, not circular
    corrFft = std::make_unique<juce::dsp::FFT>(corrOrder);
    const size_t ringSize = size_t(juce::nextPowerOfTwo(block + maxLag + 1));
    ringA.assign(ringSize, 0.0f);
    ringB.assign(ringSize, 0.0f);
    mask = ringSize - 1;
    const size_t m = size_t(1) << corrOrder;
    fa.assign(m, {});
    fb.assign(m, {});
    prod.assign(m, {});
    reset();
}

void DeltaTrace::reset()
{
    std::fill(ringA.begin(), ringA.end(), 0.0f);
    std::fill(ringB.begin(), ringB.end(), 0.0f);
    writePos = filled = sinceEstimate = 0;
    history.clear();
    delay = -1;
    specA.prepare(sr);
    specB.prepare(sr);
}

void DeltaTrace::process(const float* a, const float* b, int n)
{
    delayedB.resize(size_t(n));
    const size_t d = size_t(std::max(delay, 0));
    for (int i = 0; i < n; ++i) {
        ringA[writePos] = a[i];
        ringB[writePos] = b[i];
        delayedB[size_t(i)] = ringB[(writePos - d) & mask];
        writePos = (writePos + 1) & mask;
    }
    filled = std::min(filled + size_t(n), ringA.size());
    specA.process(a, n);
    specB.process(delayedB.data(), n);

    sinceEstimate += size_t(n);
    if (filled >= size_t(block) && sinceEstimate >= size_t(2.0 * sr)) {
        sinceEstimate = 0;
        estimateDelay();
    }
}

void DeltaTrace::estimateDelay()
{
    // The newest block of A against the same block of B: if A = B delayed by D,
    // the PHAT-weighted cross-correlation peaks at lag D.
    const size_t m = fa.size();
    std::fill(fa.begin(), fa.end(), juce::dsp::Complex<float> {});
    std::fill(fb.begin(), fb.end(), juce::dsp::Complex<float> {});
    for (int i = 0; i < block; ++i) {
        const size_t p = (writePos - size_t(block) + size_t(i)) & mask;
        fa[size_t(i)] = { ringA[p], 0.0f };
        fb[size_t(i)] = { ringB[p], 0.0f };
    }
    corrFft->perform(fa.data(), prod.data(), false);
    std::swap(fa, prod);
    corrFft->perform(fb.data(), prod.data(), false);
    std::swap(fb, prod);
    for (size_t k = 0; k < m; ++k) {
        const auto x = fa[k] * std::conj(fb[k]);
        const float mag = std::abs(x);
        fa[k] = mag > 1e-12f ? x / mag : juce::dsp::Complex<float> {};
    }
    corrFft->perform(fa.data(), prod.data(), true);

    int best = 0;
    float peak = 0, sum = 0;
    for (int lag = 0; lag <= maxLag; ++lag) {
        const float v = prod[size_t(lag)].real();
        sum += std::abs(v);
        if (v > peak) { peak = v; best = lag; }
    }
    const float mean = sum / float(maxLag + 1);
    if (peak < 8.0f * mean) return; // no clear arrival (silence, unrelated signals)

    history.push_back(best);
    if (history.size() > 5) history.erase(history.begin());
    auto sorted = history;
    std::sort(sorted.begin(), sorted.end());
    delay = sorted[sorted.size() / 2];
}

void DeltaTrace::line(float* out) const
{
    constexpr int n = grid::kPoints, half = grid::kPerOctave / 12; // ±1/12 oct = 1/6 oct window
    std::array<float, grid::kPoints> la {}, lb {}, d {};
    specA.line(Averaging::Slow, la.data());
    specB.line(Averaging::Slow, lb.data());
    for (int i = 0; i < n; ++i) d[size_t(i)] = la[size_t(i)] - lb[size_t(i)];
    for (int i = 0; i < n; ++i) {
        const int a = std::max(0, i - half), b = std::min(n - 1, i + half);
        float s = 0;
        for (int j = a; j <= b; ++j) s += d[size_t(j)];
        out[i] = s / float(b - a + 1);
    }
    const int i0 = int(grid::indexOf(200.0)), i1 = int(grid::indexOf(5000.0));
    std::vector<float> mid(out + i0, out + i1 + 1);
    std::nth_element(mid.begin(), mid.begin() + long(mid.size() / 2), mid.end());
    const float median = mid[mid.size() / 2];
    for (int i = 0; i < n; ++i) out[i] = std::clamp(out[i] - median, -40.0f, 40.0f);
}

} // namespace fs
