#include "dsp/PeakFinder.h"
#include <algorithm>
#include <cmath>

namespace fs {

namespace {
constexpr int kBaselineHalf = grid::kPerOctave / 2; // median over ±½ octave
constexpr int kNarrowHalf = grid::kPerOctave / 12;  // must drop 3 dB within ±1/12 octave
constexpr int kMatch = 2;                           // same peak if within ±1/24 octave
constexpr double kForgetSec = 0.3;                  // a track survives short dropouts
constexpr int kFirstIndex = grid::kPerOctave;       // ignore below 40 Hz (resolution too coarse)
constexpr size_t kMaxReported = 8;
}

bool PeakFinder::shouldReport(const Track& t, double nowSec, const Params& p)
{
    return t.prominence >= p.thresholdDb && nowSec - t.firstSeen >= p.holdSec;
}

void PeakFinder::update(const float* line, double now)
{
    constexpr int n = grid::kPoints;
    // Median of the ±½ octave around i. Only evaluated for points that already passed the cheap
    // local-maximum and narrowness tests, so it runs a handful of times per update, not 480.
    auto baselineAt = [&](int i) {
        const int a = std::max(0, i - kBaselineHalf), b = std::min(n - 1, i + kBaselineHalf);
        scratch.assign(line + a, line + b + 1);
        auto mid = scratch.begin() + long(scratch.size() / 2);
        std::nth_element(scratch.begin(), mid, scratch.end());
        return *mid;
    };

    for (int i = std::max(kFirstIndex, kNarrowHalf); i < n - kNarrowHalf; ++i) {
        const float v = line[i];
        if (!(v >= line[i - 1] && v > line[i + 1])) continue;
        if (v - line[i - kNarrowHalf] < 3.0f || v - line[i + kNarrowHalf] < 3.0f) continue;
        const float prominence = v - baselineAt(i);
        if (prominence < params.thresholdDb) continue;

        // Parabolic interpolation of the maximum between grid points.
        const double den = line[i - 1] - 2.0 * v + line[i + 1];
        const double index = i + (den < 0 ? 0.5 * (line[i - 1] - line[i + 1]) / den : 0.0);

        auto it = std::find_if(tracks.begin(), tracks.end(), [&](const Track& t) { return std::abs(t.index - index) <= kMatch; });
        if (it == tracks.end()) {
            tracks.push_back({ index, now, now, prominence, prominence, prominence, v });
        } else if (it->lastSeen < now) { // first match this update wins
            it->index = index;
            it->lastSeen = now;
            it->prominence = prominence;
            it->maxProminence = std::max(it->maxProminence, prominence);
            it->level = v;
        }
    }

    tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [&](const Track& t) { return now - t.lastSeen > kForgetSec; }), tracks.end());

    reported.clear();
    for (const auto& t : tracks) {
        if (t.lastSeen != now || !shouldReport(t, now, params)) continue;
        const double hz = grid::freqAt(t.index);
        reported.push_back({ float(hz), grid::noteName(hz), t.prominence, t.level, float(now - t.firstSeen) });
    }
    std::sort(reported.begin(), reported.end(), [](auto& a, auto& b) { return a.prominenceDb > b.prominenceDb; });
    if (reported.size() > kMaxReported) reported.resize(kMaxReported);
}

} // namespace fs
