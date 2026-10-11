#include <catch2/catch_test_macros.hpp>
#include "core/DeviceHub.h"
#include "core/InterfaceKey.h"

TEST_CASE("Interface keys: CoreAudio names stay plain, other drivers are prefixed")
{
    CHECK(fs::makeInterfaceKey("CoreAudio", "BlackHole 16ch") == "BlackHole 16ch");
    CHECK(fs::makeInterfaceKey("ASIO", "Focusrite USB ASIO") == "ASIO: Focusrite USB ASIO");
    CHECK(fs::makeInterfaceKey("ALSA", "US-16x08") == "ALSA: US-16x08");
}

TEST_CASE("Interface keys: split only after a known driver prefix, names round-trip exactly")
{
    const juce::StringArray win { "ASIO", "Windows Audio" };
    auto k = fs::splitInterfaceKey("ASIO: Focusrite: 2i2", win);
    CHECK(k.type == "ASIO");
    CHECK(k.name == "Focusrite: 2i2");
    k = fs::splitInterfaceKey("Windows Audio: Microphone (Realtek(R) Audio)", win);
    CHECK(k.type == "Windows Audio");
    CHECK(k.name == "Microphone (Realtek(R) Audio)");
    for (auto name : { juce::String("Focusrite: 2i2"), juce::String::fromUTF8("Mikrofon (Périphérique) é") }) {
        const auto back = fs::splitInterfaceKey(fs::makeInterfaceKey("Windows Audio", name), win);
        CHECK(back.type == "Windows Audio");
        CHECK(back.name == name);
    }
}

TEST_CASE("Interface keys: saved Mac names (no prefix) still resolve to CoreAudio")
{
    const juce::StringArray mac { "CoreAudio" };
    auto k = fs::splitInterfaceKey("BlackHole 16ch", mac);
    CHECK(k.type == "CoreAudio");
    CHECK(k.name == "BlackHole 16ch");
    k = fs::splitInterfaceKey("Foo: Bar", mac); // a Mac device whose name has ": " in it
    CHECK(k.type == "CoreAudio");
    CHECK(k.name == "Foo: Bar");
}

TEST_CASE("Interface keys: the same hardware through two drivers is two interfaces")
{
    fs::InputList l;
    l.add({ fs::makeInterfaceKey("ASIO", "UMC1820"), 0 });
    l.add({ fs::makeInterfaceKey("Windows Audio", "UMC1820"), 0 });
    const auto w = fs::wantedDevices(l, false, {});
    CHECK(w.size() == 2);
    CHECK(w.count("ASIO: UMC1820") == 1);
    CHECK(w.count("Windows Audio: UMC1820") == 1);
}
