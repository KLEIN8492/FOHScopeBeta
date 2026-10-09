#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/DbScale.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("DbScale: pan and clamp")
{
    const fs::DbScale s { 0, 90 };
    CHECK_THAT(s.panned(-10).top, WithinAbs(-10, 1e-6));
    CHECK_THAT(s.panned(100).top, WithinAbs(fs::DbScale::kMaxTop, 1e-6));
    CHECK_THAT(s.panned(-500).top, WithinAbs(fs::DbScale::kMinTop, 1e-6));
}

TEST_CASE("DbScale: zoom keeps the level under the mouse in place")
{
    const fs::DbScale s { 0, 90 };
    const float anchor = -30; // a third of the way down
    const auto z = s.zoomed(0.5f, anchor);
    CHECK_THAT(z.range, WithinAbs(45, 1e-4));
    CHECK_THAT((z.top - anchor) / z.range, WithinAbs((s.top - anchor) / s.range, 1e-5));
    CHECK_THAT(s.zoomed(0.01f, anchor).range, WithinAbs(fs::DbScale::kMinRange, 1e-6));
    CHECK_THAT(s.zoomed(100.0f, anchor).range, WithinAbs(fs::DbScale::kMaxRange, 1e-6));
}

TEST_CASE("DbScale: fit puts the loudest trace 6 dB under the top, on a 5 dB step")
{
    const fs::DbScale s { 0, 90 };
    CHECK_THAT(s.fittedTo(-44.8f).top, WithinAbs(-35, 1e-6)); // -38.8 → -35
    CHECK_THAT(s.fittedTo(-20.0f).top, WithinAbs(-10, 1e-6)); // -14 → -10
    CHECK_THAT(s.fittedTo(-44.8f).range, WithinAbs(90, 1e-6));
}

TEST_CASE("DbScale: AUTO only moves when traces leave the comfortable zone, and slowly")
{
    const fs::DbScale s { 0, 90 };
    CHECK(s.followed(-20.0f).top == 0.0f);                       // inside: untouched
    CHECK_THAT(s.followed(-70.0f).top, WithinAbs(-0.3, 1e-6));   // too low: eases down
    CHECK_THAT(s.followed(3.0f).top, WithinAbs(0.3, 1e-6));      // clipping the top: eases up
    fs::DbScale t = s;
    for (int i = 0; i < 1000; ++i) t = t.followed(-70.0f);
    CHECK(t.top <= -70.0f + 0.6f * 90.0f + 0.3f);                // ends with the trace in view
    CHECK(t.top >= -70.0f + 1.0f);
}
