#include <catch2/catch_test_macros.hpp>
#include "core/Settings.h"

TEST_CASE("Settings round-trip through XML")
{
    fs::Settings s;
    s.inputs.add(8);
    s.inputs.add(8);
    s.inputs.add(8);
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
    s.deviceState = std::make_unique<juce::XmlElement>("DEVICESETUP");
    s.deviceState->setAttribute("audioInputDeviceName", "BlackHole 16ch");

    const auto back = fs::Settings::fromXml(*s.toXml());
    CHECK(back.inputs == s.inputs);
    CHECK(back.averaging == fs::Averaging::Fast);
    CHECK(back.bandsPerOctave == 12);
    CHECK(back.showDelta);
    CHECK(back.peaks.thresholdDb == 10.0f);
    CHECK(back.peaks.holdSec == 2.5f);
    CHECK(back.splOffset == 117.25);
    CHECK(back.calibration == "calibrator");
    CHECK(back.limitLAeq15 == 100);
    CHECK(back.spectrogramSeconds == 60);
    REQUIRE(back.deviceState);
    CHECK(back.deviceState->getStringAttribute("audioInputDeviceName") == "BlackHole 16ch");
}

TEST_CASE("Settings from empty XML: one input, focused")
{
    juce::XmlElement x("FOHScope");
    const auto s = fs::Settings::fromXml(x);
    REQUIRE(s.inputs.inputs.size() == 1);
    CHECK(s.inputs.focusedId == s.inputs.inputs[0].id);
}
