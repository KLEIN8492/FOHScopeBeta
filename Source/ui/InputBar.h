#pragma once
#include "core/Inputs.h"
#include "dsp/MtwSpectrum.h"
#include "ui/Theme.h"
#include <functional>

namespace fs {

// Top bar: one chip per input (click = focus, right-click = menu), ＋ to add, then the display
// controls (averaging, band resolution, bars, delta, freeze, capture, settings).
class InputBar : public juce::Component
{
public:
    struct State
    {
        InputList inputs;
        juce::StringArray channelNames; // device input channel names
        Averaging averaging = Averaging::Slow;
        int bandsPerOctave = 3;
        bool bars = true, delta = false, frozen = false;
        int captures = 0;
    };

    std::function<void(const InputList&)> onInputs;
    std::function<void(Averaging)> onAveraging;
    std::function<void(int)> onResolution;
    std::function<void()> onBars, onDelta, onFreeze, onCapture, onClearCaptures, onSettings;

    InputBar();
    ~InputBar() override;
    void setState(const State&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class Chip;
    void rebuildChips();
    void showMenu(int id);
    void rename(int id);
    void change(const std::function<void(InputList&)>& edit);

    State state;
    std::vector<std::unique_ptr<Chip>> chips;
    juce::TextButton add { "+" };
    juce::TextButton fast { "FAST" }, slow { "SLOW" }, inf { "INF" };
    juce::ComboBox resolution;
    juce::TextButton bars { "Bars" }, delta { "Delta" }, freeze { "Freeze" }, capture { "Capture" }, settings { "Settings" };
};

} // namespace fs
