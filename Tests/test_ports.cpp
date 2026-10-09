#include <catch2/catch_test_macros.hpp>
#include "core/PortRef.h"

TEST_CASE("PortRef: LiveRack encoding round-trips any device name")
{
    for (auto name : { juce::String("Dante Virtual Soundcard"), juce::String("BlackHole 16ch"), juce::String::fromUTF8("Iñterface #2; 50% é") }) {
        const fs::PortRef p { name, 3 };
        const auto back = fs::decodePort(fs::encodePort(p));
        REQUIRE(back.has_value());
        CHECK(*back == p);
    }
    CHECK(fs::encodePort({ "Dante Virtual Soundcard", 3 }) == "Dante%20Virtual%20Soundcard#3");
    CHECK_FALSE(fs::decodePort("x#").has_value());
    CHECK_FALSE(fs::decodePort("#3").has_value());
    CHECK_FALSE(fs::decodePort("x#3a").has_value());
    CHECK(fs::PortRef { "a", 1 } < fs::PortRef { "a", 2 });
    CHECK(fs::PortRef { "a", 9 } < fs::PortRef { "b", 0 });
    CHECK_FALSE(fs::PortRef {}.valid());
}
