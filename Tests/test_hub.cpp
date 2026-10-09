#include <catch2/catch_test_macros.hpp>
#include "core/DeviceHub.h"

TEST_CASE("Hub: wanted interfaces = patched (shown) inputs, plus all while routing is on screen")
{
    fs::InputList l;
    l.add({ "Dante Virtual Soundcard", 3 });
    l.add({ "USB 2i2", 0 });
    l.add({ "BlackHole 16ch", 1 });
    l.inputs[2].muted = true; // hidden: not analysed, its interface is not needed
    l.add({});                // not patched yet
    const juce::StringArray available { "USB 2i2", "BlackHole 16ch", "MacBook Pro Microphone" };

    const auto w = fs::wantedDevices(l, false, available);
    CHECK(w == std::set<juce::String> { "Dante Virtual Soundcard", "USB 2i2" }); // Dante unplugged: still wanted (shown missing)

    const auto r = fs::wantedDevices(l, true, available);
    CHECK(r == std::set<juce::String> { "Dante Virtual Soundcard", "USB 2i2", "BlackHole 16ch", "MacBook Pro Microphone" });

    CHECK(fs::wantedDevices({}, false, available).empty());
}

TEST_CASE("Hub: reopen back-off 1, 2, 4, 8, 16, 30 s")
{
    CHECK(fs::DeviceHub::retryDelayMs(0) == 0);
    CHECK(fs::DeviceHub::retryDelayMs(1) == 1000);
    CHECK(fs::DeviceHub::retryDelayMs(2) == 2000);
    CHECK(fs::DeviceHub::retryDelayMs(4) == 8000);
    CHECK(fs::DeviceHub::retryDelayMs(6) == 30000);
    CHECK(fs::DeviceHub::retryDelayMs(20) == 30000);
}
