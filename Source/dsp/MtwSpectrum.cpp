#include "dsp/MtwSpectrum.h"
#include <cmath>

namespace fs {

void MtwSpectrum::prepare(double sampleRate)
{
    sr = sampleRate;
    // Same time windows at any rate: 32768 / 8192 / 2048 at 48 kHz, doubled at 96, ×4 at 192.
    const int boost = sr > 130000 ? 2 : sr > 65000 ? 1 : 0;
    const int orders[kWindows] = { 15 + boost, 13 + boost, 11 + boost };
    for (int w = 0; w < kWindows; ++w) {
        auto& win = windows[size_t(w)];
        win.N = 1 << orders[w];
        win.hop = win.N / 4;
        win.fft = std::make_unique<juce::dsp::FFT>(orders[w]);
        win.window.resize(size_t(win.N));
        double sumSq = 0;
        for (int i = 0; i < win.N; ++i) {
            win.window[size_t(i)] = float(0.5 - 0.5 * std::cos(2.0 * M_PI * i / win.N));
            sumSq += double(win.window[size_t(i)]) * win.window[size_t(i)];
        }
        win.scale = 2.0 / (double(win.N) * sumSq);
        win.df = sr / win.N;
        win.ring.assign(size_t(win.N), 0.0f);
        win.buf.assign(size_t(2 * win.N), 0.0f);
        const size_t bins = size_t(win.N / 2 + 1);
        win.fast.assign(bins, 0.0);
        win.slow.assign(bins, 0.0);
        win.inf.assign(bins, 0.0);
        win.aFast = 1.0 - std::exp(-win.hop / (0.125 * sr));
        win.aSlow = 1.0 - std::exp(-win.hop / (1.0 * sr));
        win.writePos = win.sinceFrame = 0;
        win.frames = win.infFrames = 0;
    }

    lineTaps.clear();
    for (int i = 0; i < grid::kPoints; ++i) {
        const double fc = grid::freqAt(i);
        lineTaps.push_back(integrate(fc * std::exp2(-0.5 / grid::kPerOctave), fc * std::exp2(0.5 / grid::kPerOctave), fc));
    }
    for (size_t r = 0; r < std::size(kResolutions); ++r) {
        const int res = kResolutions[r];
        auto& list = bandTaps[r];
        list.clear();
        for (int b = -10 * res;; ++b) {
            const double fc = 1000.0 * std::exp2(double(b) / res);
            if (fc < 19.0) continue;
            if (fc > 20500.0 || fc > 0.45 * sr) break;
            list.emplace_back(float(fc), integrate(fc * std::exp2(-0.5 / res), fc * std::exp2(0.5 / res), fc));
        }
    }
    holdDb.assign(size_t(grid::kPoints), -200.0f);
}

MtwSpectrum::Integral MtwSpectrum::integrate(double lo, double hi, double fc) const
{
    Integral out;
    out.window = windowFor(fc);
    const auto& w = windows[size_t(out.window)];
    const double df = w.df;
    // Narrower than the FFT can resolve: integrate ±1 bin around fc and rescale to the requested
    // width, so a noise density stays correct (a lone tone then reads low, as in any RTA).
    double rescale = 1.0;
    if (hi - lo < 2.0 * df) {
        rescale = (hi - lo) / (2.0 * df);
        lo = fc - df;
        hi = fc + df;
    }
    // Each bin covers [k−½, k+½]·df; weight it by how much of that falls inside [lo, hi].
    const int k0 = std::max(1, int(std::floor(lo / df - 0.5)));
    const int k1 = std::min(w.N / 2 - 1, int(std::ceil(hi / df + 0.5)));
    for (int k = k0; k <= k1; ++k) {
        const double overlap = std::min(hi, (k + 0.5) * df) - std::max(lo, (k - 0.5) * df);
        if (overlap > 0) out.taps.push_back({ k, float(w.scale * overlap / df * rescale) });
    }
    return out;
}

void MtwSpectrum::process(const float* x, int n)
{
    for (auto& w : windows) {
        // Copy in chunks that stop at the ring's end and at the next frame boundary.
        for (int i = 0; i < n;) {
            const size_t room = std::min(size_t(w.N) - w.writePos, size_t(w.hop) - w.sinceFrame);
            const size_t chunk = std::min(room, size_t(n - i));
            std::copy(x + i, x + i + long(chunk), w.ring.begin() + long(w.writePos));
            w.writePos = (w.writePos + chunk) & size_t(w.N - 1);
            w.sinceFrame += chunk;
            i += int(chunk);
            if (w.sinceFrame >= size_t(w.hop)) {
                w.sinceFrame = 0;
                w.frame();
            }
        }
    }
}

void MtwSpectrum::Window::frame()
{
    const size_t mask = size_t(N - 1);
    for (size_t i = 0; i < size_t(N); ++i) buf[i] = ring[(writePos + i) & mask] * window[i];
    std::fill(buf.begin() + N, buf.end(), 0.0f);
    fft->performFrequencyOnlyForwardTransform(buf.data(), true);
    ++frames;
    ++infFrames;
    // Warm up with a plain mean so the first second is not dragged up from zero.
    const double warm = 1.0 / double(frames);
    const double af = std::max(aFast, warm), as = std::max(aSlow, warm), ai = 1.0 / double(infFrames);
    for (size_t k = 0; k < fast.size(); ++k) {
        const double p = double(buf[k]) * buf[k];
        fast[k] += af * (p - fast[k]);
        slow[k] += as * (p - slow[k]);
        inf[k] += ai * (p - inf[k]);
    }
}

double MtwSpectrum::evaluate(const Integral& in, Averaging a) const
{
    const auto& w = windows[size_t(in.window)];
    const auto& p = a == Averaging::Fast ? w.fast : a == Averaging::Slow ? w.slow : w.inf;
    double ms = 0;
    for (const auto& t : in.taps) ms += p[size_t(t.k)] * t.weight;
    return ms > 1e-20 ? 10.0 * std::log10(ms) : -200.0;
}

void MtwSpectrum::line(Averaging a, float* out) const
{
    for (size_t i = 0; i < lineTaps.size(); ++i) out[i] = float(evaluate(lineTaps[i], a));
}

std::vector<Band> MtwSpectrum::bands(Averaging a, int bandsPerOctave) const
{
    size_t r = 1;
    for (size_t i = 0; i < std::size(kResolutions); ++i)
        if (kResolutions[i] == bandsPerOctave) r = i;
    std::vector<Band> out;
    out.reserve(bandTaps[r].size());
    for (const auto& [fc, in] : bandTaps[r]) out.push_back({ fc, float(evaluate(in, a)) });
    return out;
}

void MtwSpectrum::resetAveraging()
{
    for (auto& w : windows) {
        std::fill(w.inf.begin(), w.inf.end(), 0.0);
        w.infFrames = 0;
    }
    std::fill(holdDb.begin(), holdDb.end(), -200.0f);
}

void MtwSpectrum::updateHold(const float* line)
{
    for (size_t i = 0; i < holdDb.size(); ++i) holdDb[i] = std::max(holdDb[i], line[i]);
}

void MtwSpectrum::hold(float* out) const { std::copy(holdDb.begin(), holdDb.end(), out); }

} // namespace fs
