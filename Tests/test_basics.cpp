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

TEST_CASE("Embedded JetBrains Mono (Windows / Linux interface font) loads from the app's binary data")
{
    for (bool bold : { false, true }) {
        auto tf = fs::theme::embeddedMono(bold);
        REQUIRE(tf != nullptr);
        CHECK(tf->getName() == "JetBrains Mono");
    }
#if !JUCE_MAC
    CHECK(fs::theme::mono(11).getTypefacePtr() == fs::theme::embeddedMono(false));
#endif
}

TEST_CASE("SPL meter: each sliding Leq reports how many seconds it actually averaged")
{
    // A "15 min Leq" built from 2.5 s of audio must say so: the UI shows it as warming up and the
    // noise-limit log must not treat it as a 15-minute figure.
    constexpr double sr = 48000;
    fs::SplMeter m;
    m.prepare(sr);
    auto x = fs::test::sine(1000, 0.1, 20.0, sr);
    m.process(x.data(), int(2.5 * sr));
    auto r = m.reading();
    // Windows 1 s / 10 s / 1 / 5 / 15 min: the last T−1 whole seconds + the current partial one.
    const double early[] = { 0.5, 2.5, 2.5, 2.5, 2.5 };
    for (int w = 0; w < fs::SplMeter::kWindows; ++w) {
        INFO("window " << fs::SplMeter::kWindowSeconds[w] << " s");
        CHECK_THAT(r.leqWindowSeconds[size_t(w)], WithinAbs(early[w], 1e-9));
    }
    m.process(x.data() + int(2.5 * sr), int(17.5 * sr)); // 20 s in, on a second boundary
    r = m.reading();
    const double later[] = { 1, 10, 20, 20, 20 };
    for (int w = 0; w < fs::SplMeter::kWindows; ++w) {
        INFO("window " << fs::SplMeter::kWindowSeconds[w] << " s");
        CHECK_THAT(r.leqWindowSeconds[size_t(w)], WithinAbs(later[w], 1e-9));
    }
    // The levels themselves are unchanged: a steady sine reads the same in every window.
    for (int w = 0; w < fs::SplMeter::kWindows; ++w) CHECK_THAT(r.leqWindow[size_t(w)][0], WithinAbs(-23.01, 0.05));
}
