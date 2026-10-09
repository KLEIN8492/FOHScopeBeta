#pragma once
#include "core/Settings.h"
#include "ui/Theme.h"
#include <functional>

namespace fs {

// Settings → Inputs & Routing. A patch matrix: rows are the device's input channels (with live
// meters, so you can see where the mic actually is), columns are FOHScope inputs (name, colour,
// SPL mic, show/hide, remove). One click patches a channel to an input.
class RoutingPage : public juce::Component, private juce::Timer
{
public:
    struct Hooks
    {
        std::function<const Settings&()> get;
        std::function<void(const Settings&)> set;
        std::function<juce::StringArray()> channelNames; // device input channel names
        std::function<float(int)> takePeak;              // linear peak since last call
    };

    explicit RoutingPage(Hooks);
    ~RoutingPage() override;
    void refresh(); // settings or device changed
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    class Matrix;
    void timerCallback() override;
    void edit(const std::function<void(InputList&)>&);
    void rebuildDeltaBoxes();

    Hooks hooks;
    std::unique_ptr<Matrix> matrix;
    juce::Viewport viewport;
    juce::ComboBox deltaA, deltaB;
};

} // namespace fs
