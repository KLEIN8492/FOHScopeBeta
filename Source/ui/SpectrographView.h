#pragma once
#include "ui/Theme.h"

namespace fs {

// Scrolling time × frequency heat map of the focused input. Newest at the top; the x axis is
// the spectrum's, so a ring sits straight under its peak.
class SpectrographView : public juce::Component
{
public:
    SpectrographView() { setOpaque(true); }

    void configure(int seconds, float floorDbfs, float topDbfs);
    void addColumn(const float* gridDbRms); // grid::kPoints values, dB re FS RMS; caller repaints
    void clear();
    void setLabel(const juce::String& s, juce::Colour c) { label = s; labelColour = c; }

    void paint(juce::Graphics&) override;

private:
    static constexpr int kRowsPerSecond = 30;
    juce::Image image;
    int rows = 0, head = 0, seconds = 0;
    float floorDb = -110, topDb = -40;
    juce::String label;
    juce::Colour labelColour;
};

} // namespace fs
