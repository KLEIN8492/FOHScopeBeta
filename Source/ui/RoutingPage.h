#pragma once
#include "core/Analyser.h"
#include "core/DeviceHub.h"
#include "core/Settings.h"
#include "ui/Theme.h"
#include <functional>
#include <map>

namespace fs {

// Setup › Inputs & Routing, input first. Starts empty: + ADD INPUT makes a row; each row picks
// its interface, then a channel from that interface's strip below (every channel with a live
// meter, so you can see where the signal is). Rows also carry colour, name, SPL / SHOWN / X.
// While this page is on screen every interface is kept open for its meters.
class RoutingPage : public juce::Component, private juce::Timer
{
public:
    struct Hooks
    {
        std::function<const Settings&()> get;
        std::function<void(const Settings&)> set;
        std::function<void(bool)> setVisibleForMeters; // page shown / hidden
    };

    RoutingPage(DeviceHub&, Analyser&, Hooks);
    ~RoutingPage() override;
    void refresh(); // settings or interfaces changed
    void resized() override;
    void paint(juce::Graphics&) override;

    void addInput(); // on the last-used interface, its next free channel
    float levelDb(const PortRef&) const; // decayed meter level, −60 floor
    const DeviceHub::Info* interfaceInfo(const juce::String& name) const;

private:
    class Row;
    class Strip;
    class List;
    friend class Row;
    friend class Strip;
    friend class List;

    void timerCallback() override;
    void edit(const std::function<void(Settings&)>&);
    void rebuildDeltaBoxes();

    DeviceHub& hub;
    Analyser& analyser;
    Hooks hooks;
    std::vector<DeviceHub::Info> interfaces;
    std::map<PortRef, float> levels;
    bool shown = false;

    std::unique_ptr<List> list;
    juce::Viewport viewport;
    juce::TextButton add { "+ Add input" };
    juce::ComboBox deltaA, deltaB;
};

} // namespace fs
