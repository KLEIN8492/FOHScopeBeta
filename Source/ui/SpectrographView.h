#pragma once
#include "core/SpectrogramHistory.h"
#include "ui/Theme.h"
#include <optional>

namespace fs {

// Scrolling time × frequency heat map of the focused input. Newest at the top; the x axis is
// the spectrum's, so a ring sits straight under its peak. Keeps 15 minutes of history: scroll
// (wheel / trackpad) to go back, the view then stays parked on that moment while recording
// continues; LIVE (or scrolling forward past now) returns to live. Hover reads off a cell.
class SpectrographView : public juce::Component, public juce::SettableTooltipClient
{
public:
    SpectrographView();

    // Colour scale. SPL: the focused input is the calibrated mic, levels = dB re FS RMS + offset.
    // Otherwise dBFS (AES17). History is stored as levels, so the whole view follows a change.
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
    void clear();                           // forget the history (focused input changed, reset)
    void setLabel(const juce::String& s, juce::Colour c) { label = s; labelColour = c; }
    void setFrozen(bool);                   // freeze parks the view; recording continues
    void goLive();
    bool isLive() const { return !parkedEnd; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const;
    uint64_t viewEnd() const;          // absolute history row at the top of the view
    void park(uint64_t end);
    void redrawAll();                  // rebuild the visible image from history
    void rebuildColours();
    void drawRow(int imageRow, const uint8_t* cells);
    void paintLegend(juce::Graphics&, juce::Rectangle<float> plot);
    void paintHover(juce::Graphics&, juce::Rectangle<float> plot);
    static juce::String ago(double seconds);

    SpectrogramHistory history;
    std::optional<uint64_t> parkedEnd; // set while scrolled back / frozen
    juce::Image image;                 // the visible rows, a ring with the newest at `head`
    int rows = 0, head = 0, seconds = 0;
    Units units;
    std::array<juce::PixelARGB, 256> colourOf {}; // stored level byte → heat colour for `units`
    juce::String label;
    juce::Colour labelColour;
    juce::TextButton live { "Live" };
    juce::Point<float> hover { -1, -1 };
};

} // namespace fs
