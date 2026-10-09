#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace fs {

// Normalised biquad: y = b0·x + b1·x1 + b2·x2 − a1·y1 − a2·y2 (a0 = 1).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    static Biquad identity() { return {}; }
    bool isIdentity() const { return b0 == 1 && b1 == 0 && b2 == 0 && a1 == 0 && a2 == 0; }
    bool operator==(const Biquad&) const = default;

    std::complex<double> response(double f, double sr) const
    {
        const auto z1 = std::polar(1.0, -2.0 * M_PI * f / sr), z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }
};

inline std::complex<double> chainResponse(const Biquad* s, int n, double f, double sr)
{
    std::complex<double> h = 1.0;
    for (int i = 0; i < n; ++i) h *= s[i].response(f, sr);
    return h;
}

// Transposed Direct Form II state, double precision (stable for low crossovers at high rates).
struct BiquadState
{
    double s1 = 0, s2 = 0;
    double process(const Biquad& c, double x)
    {
        const double y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return y;
    }
    void reset() { s1 = s2 = 0; }
};

} // namespace fs
