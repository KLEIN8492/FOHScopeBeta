#include <catch2/catch_test_macros.hpp>
#include "core/LeqWarmup.h"

TEST_CASE("Leq warm-up: text shows how much of the window is averaged")
{
    auto w = fs::leqWarmup(150.0, 900);
    CHECK_FALSE(w.full);
    CHECK(w.text == "2:30 / 15:00");
    w = fs::leqWarmup(42.4, 60);
    CHECK_FALSE(w.full);
    CHECK(w.text == "0:42 / 1:00");
}

TEST_CASE("Leq warm-up: full once the window is covered (the current second may be partial)")
{
    // A full sliding window averages the last T−1 whole seconds plus the current partial one,
    // so it reads between T−1 and T seconds.
    CHECK(fs::leqWarmup(900.0, 900).full);
    CHECK(fs::leqWarmup(899.3, 900).full);
    CHECK_FALSE(fs::leqWarmup(898.0, 900).full);
    CHECK(fs::leqWarmup(899.3, 900).text.isEmpty());
}

TEST_CASE("Leq warm-up: the noise-limit log gets no 15-minute figure until the window is full")
{
    CHECK(fs::leqForLog(98.4, 120.0, 900) <= -150.0); // LevelLogger writes an empty field
    CHECK(fs::leqForLog(98.4, 899.5, 900) == 98.4);
}
