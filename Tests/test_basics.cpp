#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "TestSignals.h"
#include "dsp/FreqGrid.h"
#include "dsp/SplMeter.h"
#include "rt/SpscFifo.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("Grid: 1/48 octave from 20 Hz, note names")
{
    CHECK_THAT(fs::grid::freqAt(0), WithinAbs(20.0, 1e-9));
    CHECK_THAT(fs::grid::freqAt(48), WithinAbs(40.0, 1e-9));
    CHECK_THAT(fs::grid::indexOf(fs::grid::freqAt(100.3)), WithinAbs(100.3, 1e-9));
    CHECK(fs::grid::freqAt(fs::grid::kPoints - 1) > 20000.0);
    CHECK(fs::grid::noteName(440.0) == "A4 +0c");
    CHECK(fs::grid::noteName(2489.0).startsWith("D#7"));
    CHECK(fs::grid::noteName(27.5) == "A0 +0c");
}

TEST_CASE("SPL meter carried over from VenueTuner: weightings")
{
    constexpr double sr = 48000;
    fs::SplMeter m;
    m.prepare(sr);
    auto x = fs::test::sine(1000, 1.0, 8.0, sr);
    m.process(x.data(), int(x.size()));
    auto r = m.reading();
    CHECK_THAT(r.slow[0], WithinAbs(-3.01, 0.1));
    CHECK_THAT(r.slow[1], WithinAbs(-3.01, 0.1));
    CHECK_THAT(r.slow[2], WithinAbs(-3.01, 0.1));

    fs::SplMeter lf;
    lf.prepare(sr);
    auto y = fs::test::sine(100, 1.0, 3.0, sr);
    lf.process(y.data(), int(y.size()));
    auto q = lf.reading();
    CHECK_THAT(q.slow[1] - q.slow[0], WithinAbs(-19.1, 0.3));
}

TEST_CASE("SPSC FIFO round trip")
{
    fs::SpscFifo<float> f(8);
    float in[5] = { 1, 2, 3, 4, 5 }, out[5] {};
    CHECK(f.push(in, 5) == 5);
    CHECK(f.push(in, 5) == 3); // full
    CHECK(f.pop(out, 5) == 5);
    CHECK(out[4] == 5);
    CHECK(f.available() == 3);
}

#include "ui/Theme.h"

TEST_CASE("Shortcut text matches the platform's command key")
{
#if JUCE_MAC
    CHECK(fs::theme::shortcut(",") == "CMD+,");
#else
    CHECK(fs::theme::shortcut(",") == "CTRL+,");
#endif
}
