#pragma once
#include "ui/Theme.h"

namespace fs {

// Scrolling time × frequency heat map of the focused input. Newest at the top; the x axis is
// the spectrum's, so a ring sits straight under its peak.
class SpectrographView : public juce::Component
{
public:
    SpectrographView() { setOpaque(true); }

    // Colour scale. SPL: the focused input is the calibrated mic, levels = dB re FS RMS + offset.
    // Otherwise dBFS (AES17). Rows already drawn keep the colours of the units they were drawn in.
    struct Units
    {
        bool spl = false;
        double splOffset = 0;
        bool estimate = false; // SPL mic not calibrated
        float floor = -95, top = -25;
        bool operator==(const Units&) const = default;
    };

    void configure(int seconds, const Units&);
    void addColumn(const float* gridDbRms); // grid::kPoints values, dB re FS RMS; caller repaints
    void clear();
    void setLabel(const juce::String& s, juce::Colour c) { label = s; labelColour = c; }

    void paint(juce::Graphics&) override;

private:
    static constexpr int kRowsPerSecond = 30;
    juce::Image image;
    int rows = 0, head = 0, seconds = 0;
    Units units;
    void paintLegend(juce::Graphics&, juce::Rectangle<float> plot);
    juce::String label;
    juce::Colour labelColour;
};

} // namespace fs
