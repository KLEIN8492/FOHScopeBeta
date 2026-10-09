// Hidden benchmark ([.bench]): analysis cost for 8 inputs + delta + SPL, as % of one core in real time.
#include <catch2/catch_test_macros.hpp>
#include "TestSignals.h"
#include "core/Analyser.h"
#include <chrono>
#include <iostream>

TEST_CASE("bench: 8 inputs real-time load", "[.bench]")
{
    constexpr double sr = 48000, seconds = 20;
    constexpr int channels = 8, block = 256;
    fs::Analyser a;
    a.prepareForTest(sr, channels);
    fs::InputList l;
    for (int i = 0; i < channels; ++i) l.add(channels);
    l.deltaA = l.inputs[0].id;
    l.deltaB = l.inputs[1].id;
    a.setInputs(l);
    std::vector<std::vector<float>> sig;
    for (int c = 0; c < channels; ++c) sig.push_back(fs::test::pink(seconds, sr, 0.1f, uint32_t(c + 1)));
    std::vector<const float*> ptr(channels);
    double busy = 0;
    for (int b = 0; b < int(seconds * sr / block); ++b) {
        for (int c = 0; c < channels; ++c) ptr[size_t(c)] = sig[size_t(c)].data() + size_t(b) * block;
        a.pushTestBlock(ptr.data(), channels, block);
        if (b % 2 == 0) { // ~10 ms of audio per tick, like the real thread
            const auto t0 = std::chrono::steady_clock::now();
            a.tick();
            busy += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        }
    }
    std::cout << "8 inputs + delta + SPL: " << 100.0 * busy / seconds << " % of one core\n";
}
