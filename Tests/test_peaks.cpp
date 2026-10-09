#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "TestSignals.h"
#include "dsp/MtwSpectrum.h"
#include "dsp/PeakFinder.h"
#include <functional>

using Catch::Matchers::WithinRel;

namespace {
constexpr double sr = 48000;
constexpr int kBlock = 1600; // 33 ms, the analyser's update rate

// Runs `seconds` of signal through spectrum + finder; returns the peaks seen at any update.
std::vector<fs::Peak> run(const std::function<float(size_t)>& signal, double seconds, std::vector<fs::Peak>* last = nullptr)
{
    fs::MtwSpectrum s;
    s.prepare(sr);
    fs::PeakFinder f;
    std::vector<fs::Peak> ever;
    std::vector<float> block(kBlock), line(fs::grid::kPoints);
    size_t t = 0;
    for (int b = 0; b < int(seconds * sr / kBlock); ++b) {
        for (auto& v : block) v = signal(t++);
        s.process(block.data(), kBlock);
        s.line(fs::Averaging::Fast, line.data());
        f.update(line.data(), double(t) / sr);
        ever.insert(ever.end(), f.peaks().begin(), f.peaks().end());
    }
    if (last) *last = f.peaks();
    return ever;
}
}

TEST_CASE("Peaks: pink noise alone is never flagged")
{
    auto noise = fs::test::pink(10, sr);
    CHECK(run([&](size_t i) { return noise[i]; }, 10).empty());
}

TEST_CASE("Peaks: a tone growing out of pink noise is flagged with Hz and note")
{
    auto noise = fs::test::pink(8, sr);
    auto ringing = [&](size_t i) {
        const double t = double(i) / sr;
        const double amp = 0.0005 * std::pow(10.0, std::min(t, 6.0) / 6.0 * 40.0 / 20.0); // −40 → 0 dB over 6 s
        return noise[i] + float(amp * std::sin(2 * M_PI * 2500.0 * t));
    };
    std::vector<fs::Peak> last;
    run(ringing, 8, &last);
    REQUIRE(last.size() == 1);
    CHECK_THAT(last[0].hz, WithinRel(2500.0f, 0.01f));
    CHECK(last[0].note.startsWith("D#7"));
    CHECK(last[0].ageSec >= 1.5f);
}

TEST_CASE("Peaks: a tone shorter than the hold time is not flagged")
{
    auto noise = fs::test::pink(4, sr);
    auto blip = [&](size_t i) {
        const double t = double(i) / sr;
        return noise[i] + float((t > 1.0 && t < 2.0 ? 0.05 : 0.0) * std::sin(2 * M_PI * 2500.0 * t));
    };
    CHECK(run(blip, 4).empty());
}

TEST_CASE("Peaks: a repeating kick drum is not flagged")
{
    auto noise = fs::test::pink(10, sr);
    auto kick = [&](size_t i) {
        const double t = double(i) / sr, local = std::fmod(t, 0.5);
        const double env = local < 0.05 ? std::exp(-local / 0.015) : 0.0;
        return noise[i] + float(0.5 * env * std::sin(2 * M_PI * 60.0 * local));
    };
    CHECK(run(kick, 10).empty());
}
