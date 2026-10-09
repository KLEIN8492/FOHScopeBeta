#pragma once
#include "PinkNoise.h"
#include <cmath>
#include <vector>

namespace fs::test {

// Sine of the given peak amplitude.
inline std::vector<float> sine(double hz, double amplitude, double seconds, double sr, double phase = 0)
{
    std::vector<float> x(size_t(seconds * sr));
    for (size_t i = 0; i < x.size(); ++i) x[i] = float(amplitude * std::sin(2 * M_PI * hz * double(i) / sr + phase));
    return x;
}

inline std::vector<float> pink(double seconds, double sr, float gain = 0.1f, uint32_t seed = 1234)
{
    PinkNoise pn;
    pn.setSeed(seed);
    std::vector<float> x(size_t(seconds * sr));
    for (auto& v : x) v = pn.next() * gain;
    return x;
}

inline double db(double ms) { return 10.0 * std::log10(ms); }

} // namespace fs::test
