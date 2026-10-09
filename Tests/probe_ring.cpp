// Hidden probe ([.]): replays SignalGen's ch1 through the analyser and prints when the ring is flagged.
#include <catch2/catch_test_macros.hpp>
#include "TestSignals.h"
#include "core/Analyser.h"
#include <iostream>

TEST_CASE("probe: SignalGen ring timeline", "[.probe]")
{
    constexpr double sr = 48000;
    fs::Analyser a;
    a.prepareForTest(sr, 1);
    fs::InputList l;
    l.add(1);
    a.setInputs(l);
    auto noise = fs::test::pink(40, sr, 0.05f);
    std::vector<float> block(256);
    size_t t = 0;
    for (int b = 0; b < int(40 * sr / 256); ++b) {
        for (auto& v : block) {
            const double sec = double(t) / sr, phase = std::fmod(sec, 20.0);
            const double ring = phase > 8 && phase < 14 ? 0.0004 * std::pow(10.0, (phase - 8) / 6 * 2.0) : 0.0;
            v = noise[t] + float(ring * std::sin(2 * M_PI * 2500.0 * sec));
            ++t;
        }
        const float* p = block.data();
        a.pushTestBlock(&p, 1, 256);
        if (b % 8 == 0) {
            a.tick();
            const auto& f = a.latest();
            if (b % 64 == 0 && !f.traces.empty()) {
                const size_t k = size_t(std::lround(fs::grid::indexOf(2500)));
                std::cout << double(t) / sr << "s  line@2.5k " << f.traces[0].line[k] << "  peaks " << f.traces[0].peaks.size();
                for (auto& pk : f.traces[0].peaks) std::cout << "  [" << pk.hz << " +" << pk.prominenceDb << " age " << pk.ageSec << "]";
                std::cout << "\n";
            }
        }
    }
}
