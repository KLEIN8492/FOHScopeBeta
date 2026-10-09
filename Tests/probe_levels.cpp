// Hidden probe ([.levels]): what the spectrum line and the 1/3-oct bars show for known signals.
#include <catch2/catch_test_macros.hpp>
#include "TestSignals.h"
#include "dsp/MtwSpectrum.h"
#include <cstdio>

TEST_CASE("probe: displayed levels for known signals", "[.levels]")
{
    constexpr double sr = 48000, aes = 3.0103;
    std::vector<float> line(fs::grid::kPoints);
    for (double hz : { 50.0, 100.0, 315.0, 1000.0, 5000.0, 10000.0 }) {
        fs::MtwSpectrum s;
        s.prepare(sr);
        auto x = fs::test::sine(hz, 0.1, 6, sr); // -20 dBFS (AES17)
        s.process(x.data(), int(x.size()));
        s.line(fs::Averaging::Slow, line.data());
        float peak = -200;
        for (int i = 0; i < fs::grid::kPoints; ++i)
            if (std::abs(fs::grid::freqAt(i) / hz - 1) < 0.05) peak = std::max(peak, line[size_t(i)]);
        float band = -200;
        for (auto& b : s.bands(fs::Averaging::Slow, 3)) if (std::abs(b.fc / hz - 1) < 0.12) band = std::max(band, b.db);
        printf("sine %6.0f Hz at -20 dBFS: line peak %6.1f dBFS   1/3-oct bar %6.1f dBFS\n", hz, peak + aes, band + aes);
    }
    fs::MtwSpectrum s;
    s.prepare(sr);
    auto p = fs::test::pink(30, sr, 0.1f); // ~ -20 dBFS RMS broadband (+3 AES = -17)
    double ms = 0; for (auto v : p) ms += double(v) * v; ms /= double(p.size());
    s.process(p.data(), int(p.size()));
    s.line(fs::Averaging::Infinite, line.data());
    printf("pink noise total %.1f dBFS(AES): line at 1k %.1f dBFS, bar 1k %.1f dBFS\n", 10 * std::log10(ms) + aes,
           line[size_t(std::lround(fs::grid::indexOf(1000)))] + aes, [&] { for (auto& b : s.bands(fs::Averaging::Infinite, 3)) if (std::abs(b.fc - 1000) < 30) return b.db + aes; return -200.0; }());
}
