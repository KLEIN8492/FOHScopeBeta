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
    writeA = writeB = filledA = filledB = sinceEstimate = 0;
    history.clear();
    delay = 0;
    hasDelay = false;
    specA.prepare(sr);
    specB.prepare(sr);
}

void DeltaTrace::process(const float* a, const float* b, int n)
{
    pushA(a, n);
    pushB(b, n);
    maybeEstimate();
}

// A and B may come from different interfaces, drained at different moments: each keeps its own
// ring and write position. Whichever arrives first is held back by |delay| before its spectrum
// (B when delay > 0, A when delay < 0): a real-time stream can only be delayed, never advanced.
void DeltaTrace::pushA(const float* a, int n)
{
    for (int i = 0; i < n; ++i) ringA[(writeA + size_t(i)) & mask] = a[i];
    const size_t d = hasDelay && delay < 0 ? size_t(-delay) : 0;
    specA.process(heldBack(ringA, writeA, filledA, a, n, d), n);
    writeA = (writeA + size_t(n)) & mask;
    filledA = std::min(filledA + size_t(n), ringA.size());
    sinceEstimate += size_t(n);
}

void DeltaTrace::pushB(const float* b, int n)
{
    for (int i = 0; i < n; ++i) ringB[(writeB + size_t(i)) & mask] = b[i];
    const size_t d = hasDelay && delay > 0 ? size_t(delay) : 0;
    specB.process(heldBack(ringB, writeB, filledB, b, n, d), n);
    writeB = (writeB + size_t(n)) & mask;
    filledB = std::min(filledB + size_t(n), ringB.size());
}

// Sample i of the block just written at `write` is ring[write + i]; held back by d it becomes
// ring[write + i − d]. That slot is valid only if it was written since reset: with `filled`
// samples written before this block, i − d must be ≥ −filled. Anything older reads 0.
const float* DeltaTrace::heldBack(const std::vector<float>& ring, size_t write, size_t filled, const float* x, int n, size_t d)
{
    if (d == 0) return x;
    d = std::min(d, size_t(maxLag)); // never more than the ring keeps behind the write position
    held.resize(size_t(n));
    for (int i = 0; i < n; ++i) {
        const long back = long(i) - long(d);
        held[size_t(i)] = back >= -long(filled) ? ring[(write + size_t(i) - d) & mask] : 0.0f;
    }
    return held.data();
}

// Call once both streams are pushed up to the same moment (end of an analysis tick).
void DeltaTrace::maybeEstimate()
{
    if (filledA >= size_t(block) && filledB >= size_t(block) && sinceEstimate >= size_t(2.0 * sr)) {
        sinceEstimate = 0;
        estimateDelay();
    }
}

void DeltaTrace::estimateDelay()
{
    // The newest block of A against the same block of B: if A = B delayed by D, the PHAT-weighted
    // cross-correlation peaks at lag D. Negative lags (A ahead of B) sit at the top of the
    // zero-padded buffer, at index m − |D|: the padding keeps both halves free of wrap-around.
    const size_t m = fa.size();
    std::fill(fa.begin(), fa.end(), juce::dsp::Complex<float> {});
    std::fill(fb.begin(), fb.end(), juce::dsp::Complex<float> {});
    for (int i = 0; i < block; ++i) {
        fa[size_t(i)] = { ringA[(writeA - size_t(block) + size_t(i)) & mask], 0.0f };
        fb[size_t(i)] = { ringB[(writeB - size_t(block) + size_t(i)) & mask], 0.0f };
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
    for (int lag = -maxLag; lag <= maxLag; ++lag) {
        const float v = prod[lag >= 0 ? size_t(lag) : m - size_t(-lag)].real();
        sum += std::abs(v);
        if (v > peak) { peak = v; best = lag; }
    }
    const float mean = sum / float(2 * maxLag + 1);
    if (peak < 8.0f * mean) return; // no clear arrival (silence, unrelated signals)

    history.push_back(best);
    if (history.size() > 5) history.erase(history.begin());
    auto sorted = history;
    std::sort(sorted.begin(), sorted.end());
    delay = sorted[sorted.size() / 2];
    hasDelay = true;
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
