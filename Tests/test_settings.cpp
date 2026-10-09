#include <catch2/catch_test_macros.hpp>
#include "core/Settings.h"

TEST_CASE("Settings v2 round-trip through XML")
{
    fs::Settings s;
    s.inputs.add({ "Dante Virtual Soundcard", 4 });
    s.inputs.add({ "BlackHole 16ch", 0 });
    s.inputs.add({ "BlackHole 16ch", 1 });
    s.inputs.inputs[1].name = "Desk L";
    s.inputs.inputs[2].muted = true;
    s.inputs.setSplMic(s.inputs.inputs[0].id);
    s.inputs.deltaA = 1;
    s.inputs.deltaB = 2;
    s.inputs.focusedId = 2;
    s.averaging = fs::Averaging::Fast;
    s.bandsPerOctave = 12;
    s.showDelta = true;
    s.peaks = { 10.0f, 2.5f };
    s.splOffset = 117.25;
    s.calibration = "calibrator";
    s.limitLAeq15 = 100;
    s.spectrogramSeconds = 60;
    s.sampleRate = 96000;
    s.bufferSize = 128;
    s.lastDevice = "BlackHole 16ch";

    const auto back = fs::Settings::fromXml(*s.toXml());
    CHECK(back.inputs == s.inputs);
    CHECK(back.inputs.inputs[0].port == fs::PortRef { "Dante Virtual Soundcard", 4 });
    CHECK(back.averaging == fs::Averaging::Fast);
    CHECK(back.bandsPerOctave == 12);
    CHECK(back.showDelta);
    CHECK(back.peaks.thresholdDb == 10.0f);
    CHECK(back.peaks.holdSec == 2.5f);
    CHECK(back.splOffset == 117.25);
    CHECK(back.calibration == "calibrator");
    CHECK(back.limitLAeq15 == 100);
    CHECK(back.spectrogramSeconds == 60);
    CHECK(back.sampleRate == 96000);
    CHECK(back.bufferSize == 128);
    CHECK(back.lastDevice == "BlackHole 16ch");
}

TEST_CASE("Settings v1 migrates: bare channels land on the old device, format carried over")
{
    auto x = juce::XmlDocument::parse(R"(<FOHScope version="1">
        <Inputs focused="1" deltaA="-1" deltaB="-1" nextColour="2">
          <Input id="1" name="FOH mic" channel="2" colour="fffa9a45" muted="0" splMic="1"/>
        </Inputs>
        <DEVICESETUP deviceType="CoreAudio" audioInputDeviceName="BlackHole 16ch" audioDeviceRate="44100" audioDeviceBufferSize="512"/>
      </FOHScope>)");
    REQUIRE(x);
    const auto s = fs::Settings::fromXml(*x);
    REQUIRE(s.inputs.inputs.size() == 1);
    CHECK(s.inputs.inputs[0].port == fs::PortRef { "BlackHole 16ch", 2 });
    CHECK(s.inputs.inputs[0].splMic);
    CHECK(s.sampleRate == 44100);
    CHECK(s.bufferSize == 512);
    CHECK(s.lastDevice == "BlackHole 16ch");

    auto y = juce::XmlDocument::parse(R"(<FOHScope version="1"><Inputs><Input id="1" name="x" channel="0"/></Inputs></FOHScope>)");
    const auto t = fs::Settings::fromXml(*y);
    CHECK_FALSE(t.inputs.inputs[0].port.valid()); // no known device: shown as not patched
}

TEST_CASE("Settings from empty XML: no inputs")
{
    juce::XmlElement x("FOHScope");
    const auto s = fs::Settings::fromXml(x);
    CHECK(s.inputs.inputs.empty());
    CHECK(s.inputs.focusedId == 0);
}
