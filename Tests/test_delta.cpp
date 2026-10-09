#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "TestSignals.h"
#include "dsp/Biquad.h"
#include "dsp/DeltaTrace.h"

using Catch::Matchers::WithinAbs;

namespace {
// RBJ cookbook high shelf.
fs::Biquad highShelf(double f, double gainDb, double sr)
{
    const double A = std::pow(10.0, gainDb / 40.0), w0 = 2 * M_PI * f / sr, cs = std::cos(w0), sn = std::sin(w0);
    const double al = sn / 2 * std::sqrt(2.0), r = 2 * std::sqrt(A) * al;
    const double a0 = (A + 1) - (A - 1) * cs + r;
    return { A * ((A + 1) + (A - 1) * cs + r) / a0, -2 * A * ((A - 1) + (A + 1) * cs) / a0, A * ((A + 1) + (A - 1) * cs - r) / a0,
             2 * ((A - 1) - (A + 1) * cs) / a0, ((A + 1) - (A - 1) * cs - r) / a0 };
}
}

TEST_CASE("Delta: finds the mic's arrival time and shows what the room adds")
{
    constexpr double sr = 48000;
    constexpr int delay = 1440; // 30 ms
    auto b = fs::test::pink(8, sr);
    std::vector<float> a(b.size());
    const auto shelf = highShelf(2000, 6, sr);
    fs::BiquadState st;
    for (size_t i = 0; i < a.size(); ++i) a[i] = 0.5f * float(st.process(shelf, i >= delay ? b[i - delay] : 0.0f));

    fs::DeltaTrace d;
    d.prepare(sr);
    CHECK_FALSE(d.valid());
    for (size_t i = 0; i + 480 <= a.size(); i += 480) d.process(a.data() + i, b.data() + i, 480);
    REQUIRE(d.valid());
    CHECK_THAT(d.delayMs(), WithinAbs(30.0, 0.1));

    std::vector<float> line(fs::grid::kPoints);
    d.line(line.data());
    auto at = [&](double hz) { return line[size_t(std::lround(fs::grid::indexOf(hz)))]; };
    CHECK_THAT(at(8000) - at(400), WithinAbs(6.0, 1.0));
    for (double hz = 300; hz <= 800; hz *= 1.1) CHECK_THAT(at(hz), WithinAbs(0.0, 1.0));
}

TEST_CASE("Delta: unrelated signals never produce a delay")
{
    constexpr double sr = 48000;
    auto a = fs::test::pink(6, sr, 0.1f, 1), b = fs::test::pink(6, sr, 0.1f, 99);
    fs::DeltaTrace d;
    d.prepare(sr);
    for (size_t i = 0; i + 480 <= a.size(); i += 480) d.process(a.data() + i, b.data() + i, 480);
    CHECK_FALSE(d.valid());
}
