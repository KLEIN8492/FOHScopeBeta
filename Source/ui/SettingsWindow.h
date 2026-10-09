#pragma once
#include "core/Settings.h"
#include "ui/AudioPage.h"
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
        std::function<void(bool)> setRoutingVisible; // routing page on screen: keep every interface open
    };

    SettingsWindow(DeviceHub&, Analyser&, Hooks);
    ~SettingsWindow() override;
    void closeButtonPressed() override { setVisible(false); }
    void refresh(); // settings changed elsewhere
    void addInput(); // shows Inputs & Routing and adds a row

private:
    class Measurement;
    std::unique_ptr<RoutingPage> routing;
    std::unique_ptr<AudioPage> audio;
    juce::TabbedComponent* tabs = nullptr;
    std::unique_ptr<Measurement> measurement;
};

} // namespace fs
