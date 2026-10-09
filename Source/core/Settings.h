#pragma once
#include "core/Inputs.h"
#include "dsp/MtwSpectrum.h"
#include "dsp/PeakFinder.h"
#include <juce_core/juce_core.h>
#include <memory>

namespace fs {

// Everything FOHScope remembers between launches. Message thread only.
struct Settings
{
    InputList inputs;
    Averaging averaging = Averaging::Slow;
    int bandsPerOctave = 3;
    bool showBars = true, showDelta = false;
    PeakFinder::Params peaks;

    // dB SPL = dB re FS RMS + splOffset. 120 is a typical guess for an uncalibrated mic.
    double splOffset = 120.0;
    juce::String calibration = "estimate"; // calibrator | offset | estimate
    double limitLAeq15 = 102.0, limitLCeq15 = 118.0;

    int spectrogramSeconds = 30;          // 10 / 30 / 60
    float spectrogramFloor = -95.0f, spectrogramTop = -25.0f; // dBFS (AES17) colour range
    float displayTop = 0.0f, displayRange = 90.0f;             // spectrum y axis, dBFS

    // Audio: every interface opens at this shared rate / buffer (no resampling).
    double sampleRate = 48000.0;
    int bufferSize = 256;
    juce::String lastDevice; // interface the next new input starts on

    std::unique_ptr<juce::XmlElement> toXml() const;
    static Settings fromXml(const juce::XmlElement&);

    static juce::File directory(); // ~/Library/Application Support/FOHScope
    static Settings load();        // defaults (no inputs) when missing or unreadable
    bool save() const;
};

} // namespace fs
