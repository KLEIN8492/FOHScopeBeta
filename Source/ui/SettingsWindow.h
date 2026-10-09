#pragma once
#include "core/Settings.h"
#include "ui/Theme.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <optional>

namespace fs {

// ⌘, : Audio (device / rate / buffer / channels) and Measurement (delta, calibration, limits,
// feedback finder, spectrograph, display range).
class SettingsWindow : public juce::DocumentWindow
{
public:
    struct Hooks
    {
        std::function<const Settings&()> get;
        std::function<void(const Settings&)> set;
        std::function<std::optional<double>()> splMicZSlow; // dB re FS RMS of the SPL mic now
    };

    SettingsWindow(juce::AudioDeviceManager&, Hooks);
    ~SettingsWindow() override;
    void closeButtonPressed() override { setVisible(false); }
    void refresh(); // settings changed elsewhere

private:
    class Measurement;
    std::unique_ptr<Measurement> measurement;
};

} // namespace fs
