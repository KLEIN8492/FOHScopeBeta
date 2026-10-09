#pragma once
#include "core/DeviceHub.h"
#include "core/Settings.h"
#include "ui/Theme.h"
#include <functional>

namespace fs {

// Setup › Audio: the shared sample rate and buffer size every interface opens at, and the state
// of every input interface on the Mac. Interfaces are chosen per input (Inputs & Routing).
class AudioPage : public juce::Component
{
public:
    struct Hooks
    {
        std::function<const Settings&()> get;
        std::function<void(const Settings&)> set;
    };

    AudioPage(DeviceHub&, Hooks);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DeviceHub& hub;
    Hooks hooks;
    std::vector<DeviceHub::Info> interfaces;
    juce::ComboBox rate, buffer;
};

} // namespace fs
