#pragma once
#include "core/Frame.h"
#include "core/Inputs.h"
#include "ui/Theme.h"

namespace fs {

// Right column: calibrated SPL (with limit colouring) and the feedback / peak list.
class SidePanel : public juce::Component
{
public:
    struct Spl
    {
        double limitLAeq15 = 102, limitLCeq15 = 118;
        juce::String calibration; // calibrator | offset | estimate
        juce::String logStatus;   // empty = logging fine
    };

    void setData(const Frame&, const InputList&, const Spl&);
    void paint(juce::Graphics&) override;

private:
    void paintSpl(juce::Graphics&, juce::Rectangle<float>);
    void paintPeaks(juce::Graphics&, juce::Rectangle<float>);

    Frame frame;
    InputList inputs;
    Spl spl;
};

} // namespace fs
