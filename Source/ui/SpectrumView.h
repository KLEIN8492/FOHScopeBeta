#pragma once
#include "core/Frame.h"
#include "core/DbScale.h"
#include "core/Inputs.h"
#include "ui/Theme.h"
#include <functional>
#include <optional>

namespace fs {

// Spectrum: one hi-res line per input, RTA bars + peak hold for the focused one, captured
// reference traces, feedback markers, the A − B delta strip and a hover / pinned cursor.
class SpectrumView : public juce::Component
{
public:
    struct Capture
    {
        GridLine line;
        juce::Colour colour;
        juce::String label;
    };

    struct View
    {
        bool showBars = true, showDelta = false, frozen = false;
        float top = 0.0f, range = 90.0f; // dBFS
        int bandsPerOctave = 3;
    };

    void setData(const Frame&, const InputList&, const View&);
    void setCaptures(const std::vector<Capture>* c) { captures = c; repaint(); }
    void setScale(DbScale s); // redraws without new data (also while frozen)

    // dB scale gestures: wheel = pan, cmd+wheel / pinch = zoom, drag the dB labels = pan,
    // double-click the dB labels = fit.
    std::function<void(DbScale)> onScale;
    std::function<void()> onFit;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify(const juce::MouseEvent&, float scaleFactor) override;

private:
    float y(float dbfs) const;
    float dbAt(float py) const;
    DbScale scale() const { return { view.top, view.range }; }
    bool onAxis(juce::Point<float> p) const { return p.x < plot.getX() && p.y >= plot.getY() - 10 && p.y <= plot.getBottom() + 10; }
    juce::Path tracePath(const GridLine&) const;
    void paintGrid(juce::Graphics&);
    void paintDelta(juce::Graphics&, juce::Rectangle<float>);
    void paintCursor(juce::Graphics&, float x, bool pinned);

    Frame frame;
    InputList inputs;
    View view;
    const std::vector<Capture>* captures = nullptr;
    juce::Rectangle<float> plot, deltaArea;
    juce::Image gridCache;
    juce::Point<float> gridKey;
    float gridScale = 0;
    theme::FreqAxis axis;
    float hoverX = -1, pinnedHz = -1;
    std::optional<DbScale> dragStart;
};

} // namespace fs
