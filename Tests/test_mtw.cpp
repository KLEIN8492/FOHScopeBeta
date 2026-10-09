#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "TestSignals.h"
#include "dsp/FreqGrid.h"
#include "dsp/MtwSpectrum.h"
#include <algorithm>

using Catch::Matchers::WithinAbs;
using fs::Averaging;

static const fs::Band& bandAt(const std::vector<fs::Band>& b, double hz)
{
    return *std::min_element(b.begin(), b.end(), [hz](auto& x, auto& y) { return std::abs(std::log(x.fc / hz)) < std::abs(std::log(y.fc / hz)); });
}

TEST_CASE("MTW: a sine reads its exact level in its band")
{
    const double sr = GENERATE(48000.0, 96000.0);
    fs::MtwSpectrum s;
    s.prepare(sr);
    for (double hz : { 63.0, 1000.0, 6300.0 }) {
        s.prepare(sr);
        auto x = fs::test::sine(hz, 0.1, 5.0, sr);
        s.process(x.data(), int(x.size()));
        auto b = s.bands(Averaging::Slow, 3);
        INFO("sr " << sr << " hz " << hz);
        const auto& own = bandAt(b, hz);
        CHECK_THAT(own.db, WithinAbs(-23.01, 0.1)); // ms = 0.1² / 2
        CHECK(bandAt(b, hz * 1.6).db < own.db - 40);
        CHECK(bandAt(b, hz / 1.6).db < own.db - 40);
    }
}

TEST_CASE("MTW: pink noise is flat in 1/3-oct bands and on the 1/48 line, across window crossovers")
{
    const double sr = GENERATE(48000.0, 96000.0);
    fs::MtwSpectrum s;
    s.prepare(sr);
    // 90 s: below ~150 Hz a line point integrates only 2 bins of the long window, so it needs
    // many frames before its random spread drops well under the ±1 dB tolerance.
    auto x = fs::test::pink(90.0, sr);
    s.process(x.data(), int(x.size()));
    INFO("sr " << sr);

    auto b = s.bands(Averaging::Infinite, 3);
    std::vector<float> in;
    for (auto& band : b) if (band.fc > 30 && band.fc < 16500) in.push_back(band.db);
    REQUIRE(in.size() == 28);
    std::sort(in.begin(), in.end());
    const float median = in[in.size() / 2];
    for (auto v : in) CHECK_THAT(v, WithinAbs(median, 0.5));

    std::vector<float> line(fs::grid::kPoints);
    s.line(Averaging::Infinite, line.data());
    const int i0 = int(fs::grid::indexOf(40)), i1 = int(fs::grid::indexOf(16000));
    std::vector<float> seg(line.begin() + i0, line.begin() + i1);
    std::sort(seg.begin(), seg.end());
    const float lineMedian = seg[seg.size() / 2];
    for (int i = i0; i < i1; ++i) {
        INFO("f " << fs::grid::freqAt(i));
        CHECK_THAT(line[size_t(i)], WithinAbs(lineMedian, 1.0));
    }
    // Band power per 1/3 octave = 16 × the 1/48-octave density.
    CHECK_THAT(median - lineMedian, WithinAbs(10 * std::log10(16.0), 0.5));
}

TEST_CASE("MTW: silence reads the floor, peak hold keeps the maximum until reset")
{
    fs::MtwSpectrum s;
    s.prepare(48000);
    CHECK_FALSE(s.hasData());
    std::vector<float> z(48000, 0.0f), line(fs::grid::kPoints), hold(fs::grid::kPoints);
    s.process(z.data(), int(z.size()));
    CHECK(s.hasData());
    s.line(Averaging::Fast, line.data());
    CHECK(line[200] <= -150);

    auto x = fs::test::sine(1000, 0.5, 1.0, 48000);
    s.process(x.data(), int(x.size()));
    s.line(Averaging::Fast, line.data());
    s.updateHold(line.data());
    std::vector<float> z3(3 * 48000, 0.0f);
    s.process(z3.data(), int(z3.size()));
    s.line(Averaging::Fast, line.data());
    s.updateHold(line.data());
    s.hold(hold.data());
    const size_t k = size_t(std::lround(fs::grid::indexOf(1000)));
    CHECK(hold[k] > -15);
    CHECK(line[k] < -60);
    s.resetAveraging();
    s.hold(hold.data());
    CHECK(hold[k] <= -150);
}
