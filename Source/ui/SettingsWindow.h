#pragma once
#include "core/Settings.h"
#include "ui/RoutingPage.h"
#include "ui/Theme.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <optional>

namespace fs {

// ⌘, : Inputs & Routing (patch matrix with meters, delta A/B), Audio (device / rate / buffer)
// and Measurement (calibration, limits, feedback finder, spectrograph, display range).
class SettingsWindow : public juce::DocumentWindow
{
public:
    struct Hooks
    {
        std::function<const Settings&()> get;
        std::function<void(const Settings&)> set;
        // Slow level of the SPL mic now, dB re FS RMS: weighting 0 = Z, 1 = A, 2 = C.
        std::function<std::optional<double>(int weighting)> splMicSlow;
        std::function<juce::StringArray()> channelNames; // device input channel names
        std::function<float(int)> takePeak;              // routing meters: linear peak since last call
    };

    SettingsWindow(juce::AudioDeviceManager&, Hooks);
    ~SettingsWindow() override;
    void closeButtonPressed() override { setVisible(false); }
    void refresh(); // settings changed elsewhere

private:
    class Measurement;
    std::unique_ptr<RoutingPage> routing;
    std::unique_ptr<Measurement> measurement;
};

} // namespace fs
