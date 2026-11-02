#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/SpectrogramHistory.h"
#include "dsp/FreqGrid.h"
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {
std::vector<float> flat(float db) { return std::vector<float>(size_t(fs::grid::kPoints), db); }
}

TEST_CASE("Spectrogram history: levels are kept to 0.5 dB, silence and overload stay readable")
{
    for (float db : { -120.0f, -95.3f, -47.8f, -20.0f, -3.01f, 0.0f, 1.5f }) {
        INFO(db << " dB");
        CHECK_THAT(fs::SpectrogramHistory::dequantize(fs::SpectrogramHistory::quantize(db)), WithinAbs(db, 0.25));
    }
    CHECK(fs::SpectrogramHistory::dequantize(fs::SpectrogramHistory::quantize(-200.0f)) <= -125.0f); // silence: bottom
    CHECK(fs::SpectrogramHistory::dequantize(fs::SpectrogramHistory::quantize(30.0f)) >= 2.0f);      // overload: top
}

TEST_CASE("Spectrogram history: newest first, oldest dropped when full")
{
    fs::SpectrogramHistory h(100);
    CHECK(h.size() == 0);
    CHECK(h.level(0, 10) <= -199.0f); // nothing recorded yet
    for (int i = 0; i < 130; ++i) h.push(flat(-100.0f + float(i) * 0.5f).data());
    CHECK(h.size() == 100);
    CHECK(h.pushed() == 130);
    CHECK_THAT(h.level(0, 0), WithinAbs(-100.0 + 129 * 0.5, 0.25));  // newest = row 129
    CHECK_THAT(h.level(99, 479), WithinAbs(-100.0 + 30 * 0.5, 0.25)); // oldest kept = row 30
    CHECK(h.level(100, 0) <= -199.0f);                                // beyond what is kept
    h.clear();
    CHECK(h.size() == 0);
    CHECK(h.pushed() == 0);
}

TEST_CASE("Spectrogram history: a parked view stays on its moment and can't scroll past the ends")
{
    fs::SpectrogramHistory h(1000);
    for (int i = 0; i < 600; ++i) h.push(flat(-60.0f).data());
    const int visible = 300;
    // Parked 100 rows back: the newest visible row is absolute row 499.
    auto end = h.clampEnd(599 - 100, visible);
    CHECK(end == 499);
    // More audio arrives: a parked end is absolute, so it still points at the same moment.
    for (int i = 0; i < 50; ++i) h.push(flat(-60.0f).data());
    CHECK(h.rowsAgo(end) == 649 - 499);
    // Can't scroll back past the oldest kept row (the view must stay full) nor past now.
    CHECK(h.clampEnd(0, visible) == uint64_t(visible - 1));
    CHECK(h.clampEnd(10'000, visible) == 649);
    CHECK(h.isLive(649));
    CHECK_FALSE(h.isLive(648));
}
