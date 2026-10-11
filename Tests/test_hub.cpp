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

TEST_CASE("Hub: the routing page never auto-opens ASIO drivers (each one loads the hardware driver)")
{
    fs::InputList l;
    l.add({ "ASIO: Focusrite USB ASIO", 0 }); // patched: wanted
    const juce::StringArray available { "ASIO: Focusrite USB ASIO", "ASIO: ASIO4ALL v2", "Windows Audio: Speakers", "ALSA: US-16x08" };
    const auto w = fs::wantedDevices(l, true, available);
    CHECK(w.count("ASIO: Focusrite USB ASIO") == 1);
    CHECK(w.count("ASIO: ASIO4ALL v2") == 0);
    CHECK(w.count("Windows Audio: Speakers") == 1);
    CHECK(w.count("ALSA: US-16x08") == 1);
}
