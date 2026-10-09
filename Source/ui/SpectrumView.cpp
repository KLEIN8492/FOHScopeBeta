#include "ui/SpectrumView.h"

namespace fs {

using namespace theme;

void SpectrumView::setData(const Frame& f, const InputList& l, const View& v)
{
    const bool relayout = v.showDelta != view.showDelta;
    frame = f;
    inputs = l;
    view = v;
    if (relayout) resized();
    repaint();
}

void SpectrumView::resized()
{
    // Spectrum on top, its frequency labels, then (optionally) the delta strip in its own space.
    auto r = getLocalBounds().toFloat().withTrimmedLeft(float(kPlotLeft)).withTrimmedRight(float(kPlotRight)).withTrimmedTop(18);
    deltaArea = view.showDelta ? r.removeFromBottom(juce::jlimit(90.0f, 180.0f, r.getHeight() * 0.3f)) : juce::Rectangle<float>();
    plot = r.withTrimmedBottom(20);
    gridCache = {};
    axis = { plot.getX(), plot.getRight() };
}

float SpectrumView::y(float dbfs) const
{
    return plot.getY() + plot.getHeight() * (view.top - dbfs) / view.range;
}

juce::Path SpectrumView::tracePath(const GridLine& values) const
{
    juce::Path p;
    bool started = false;
    for (int i = 0; i < grid::kPoints; ++i) {
        const float v = values[size_t(i)];
        if (v < -190.0f) { started = false; continue; }
        const float px = axis.x(grid::freqAt(i)), py = juce::jlimit(plot.getY(), plot.getBottom(), y(v + kAes17));
        if (!started) { p.startNewSubPath(px, py); started = true; }
        else p.lineTo(px, py);
    }
    return p;
}

void SpectrumView::paintGrid(juce::Graphics& g)
{
    g.setFont(mono(10.5f));
    for (double hz : { 20.0, 30.0, 40.0, 50.0, 60.0, 80.0, 100.0, 200.0, 300.0, 400.0, 500.0, 600.0, 800.0, 1000.0, 2000.0, 3000.0,
                       4000.0, 5000.0, 6000.0, 8000.0, 10000.0, 20000.0 }) {
        const bool major = hz == 20 || hz == 100 || hz == 1000 || hz == 10000 || hz == 20000 || hz == 50 || hz == 500 || hz == 5000
                           || hz == 200 || hz == 2000;
        const float x = axis.x(hz);
        g.setColour(major ? gridStrong : gridLine);
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        if (major) {
            g.setColour(dim);
            g.drawText(hzText(hz), juce::Rectangle<float>(x - 20, plot.getBottom() + 3, 40, 14), juce::Justification::centred);
        }
    }
    for (float db = std::floor(view.top / 10) * 10; db >= view.top - view.range; db -= 10) {
        const float py = y(db);
        g.setColour(int(db) % 20 == 0 ? gridStrong : gridLine);
        g.drawHorizontalLine(juce::roundToInt(py), plot.getX(), plot.getRight());
        g.setColour(dim);
        g.drawText(juce::String(int(db)), juce::Rectangle<float>(0, py - 7, float(kPlotLeft) - 6, 14), juce::Justification::centredRight);
    }
    g.setColour(dim);
    g.drawText("dBFS", juce::Rectangle<float>(4, 2, float(kPlotLeft), 14), juce::Justification::centredLeft);
}

void SpectrumView::paint(juce::Graphics& g)
{
    // The grid (lines + labels) only changes with size or range: render it once, blit it per frame.
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (!gridCache.isValid() || gridKey != juce::Point<float>(view.top, view.range) || gridScale != scale) {
        gridKey = { view.top, view.range };
        gridScale = scale;
        gridCache = juce::Image(juce::Image::RGB, juce::roundToInt(float(getWidth()) * scale), juce::roundToInt(float(getHeight()) * scale), true);
        juce::Graphics cg(gridCache);
        cg.addTransform(juce::AffineTransform::scale(scale));
        cg.fillAll(bg);
        paintGrid(cg);
    }
    g.drawImage(gridCache, getLocalBounds().toFloat());
    g.saveState();
    g.reduceClipRegion(plot.toNearestInt());

    const auto* focusedInput = inputs.find(inputs.focusedId);
    const auto* focused = focusedInput ? frame.trace(focusedInput->id) : nullptr;

    // RTA bars of the focused input.
    if (view.showBars && focused && focused->hasData && focusedInput) {
        const double halfWidth = std::exp2(0.5 / view.bandsPerOctave);
        for (const auto& b : frame.bands) {
            if (b.db < -190) continue;
            const float x0 = axis.x(b.fc / halfWidth) + 1, x1 = axis.x(b.fc * halfWidth) - 1;
            const float top = juce::jlimit(plot.getY(), plot.getBottom(), y(b.db + kAes17));
            g.setColour(focusedInput->colour.withAlpha(0.18f));
            g.fillRect(juce::Rectangle<float>(x0, top, x1 - x0, plot.getBottom() - top));
            g.setColour(focusedInput->colour.withAlpha(0.55f));
            g.drawHorizontalLine(juce::roundToInt(top), x0, x1);
        }
    }

    // Captured reference traces.
    if (captures) {
        const float dashes[] = { 5.0f, 4.0f };
        for (const auto& c : *captures) {
            juce::Path dashed;
            juce::PathStrokeType(1.3f).createDashedStroke(dashed, tracePath(c.line), dashes, 2);
            g.setColour(c.colour.withAlpha(0.75f));
            g.fillPath(dashed);
        }
    }

    // Peak hold (focused) then every live trace, focused last so it sits on top.
    if (focused && focused->hasData && focusedInput) {
        // Thin translucent line: a dashed stroke of 480 points costs more than all traces together.
        g.setColour(focusedInput->colour.withAlpha(0.35f));
        g.strokePath(tracePath(focused->hold), juce::PathStrokeType(1.0f));
    }
    auto drawTrace = [&](const TraceFrame& t, const Input& in, bool isFocused) {
        if (!t.hasData) return;
        g.setColour(in.colour.withAlpha(isFocused ? 1.0f : 0.8f));
        g.strokePath(tracePath(t.line), juce::PathStrokeType(isFocused ? 2.0f : 1.4f));
    };
    for (const auto& t : frame.traces)
        if (const auto* in = inputs.find(t.id); in && t.id != inputs.focusedId) drawTrace(t, *in, false);
    if (focused && focusedInput) drawTrace(*focused, *focusedInput, true);

    // Feedback markers on every trace.
    g.setFont(mono(11.0f));
    for (const auto& t : frame.traces) {
        const auto* in = inputs.find(t.id);
        if (!in) continue;
        for (const auto& p : t.peaks) {
            const float px = axis.x(p.hz), py = juce::jlimit(plot.getY() + 22, plot.getBottom(), y(p.levelDb + kAes17)) - 6;
            juce::Path tri;
            tri.addTriangle(px - 5, py - 9, px + 5, py - 9, px, py);
            g.setColour(red);
            g.fillPath(tri);
            g.setColour(text);
            g.drawText(hzText(p.hz), juce::Rectangle<float>(px - 30, py - 24, 60, 14), juce::Justification::centred);
        }
    }

    g.restoreState();
    if (view.showDelta) paintDelta(g, deltaArea);

    if (pinnedHz > 0) paintCursor(g, axis.x(pinnedHz), true);
    if (hoverX >= plot.getX() && hoverX <= plot.getRight()) paintCursor(g, hoverX, false);

    if (inputs.inputs.empty()) {
        g.setColour(dim);
        g.setFont(mono(13));
        g.drawText("NO INPUTS  -  CMD+, TO ADD ONE", plot, juce::Justification::centred);
    }

    if (view.frozen) {
        auto badge = juce::Rectangle<float>(plot.getRight() - 86, plot.getY() + 6, 80, 22);
        g.setColour(amber);
        g.fillRect(badge);
        g.setColour(bg);
        g.setFont(mono(11, true));
        g.drawText("FROZEN", badge, juce::Justification::centred);
    }
}

void SpectrumView::paintDelta(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(panel);
    g.fillRect(r);
    const auto* a = inputs.find(inputs.deltaA);
    const auto* b = inputs.find(inputs.deltaB);
    g.setFont(mono(11.0f));
    g.setColour(dim);
    juce::String label = a && b ? a->name + utf8(" − ") + b->name : juce::String("Delta: pick A and B (right-click an input)");
    if (a && b) label += frame.deltaValid ? "   delay " + juce::String(frame.deltaMs, 1) + " ms" : utf8("   waiting for a correlated signal…");
    g.drawText(label, r.reduced(6, 2).removeFromTop(14), juce::Justification::centredLeft);

    for (double hz : { 100.0, 1000.0, 10000.0 }) {
        g.setColour(gridLine);
        g.drawVerticalLine(juce::roundToInt(axis.x(hz)), r.getY() + 16, r.getBottom());
    }
    const float mid = r.getCentreY() + 8, scale = (r.getHeight() - 24) / 24.0f; // ±12 dB
    for (int db : { -12, -6, 0, 6, 12 }) {
        const float py = mid - float(db) * scale;
        g.setColour(db == 0 ? gridStrong : gridLine);
        g.drawHorizontalLine(juce::roundToInt(py), r.getX(), r.getRight());
        g.setColour(dim);
        g.drawText((db > 0 ? "+" : "") + juce::String(db), juce::Rectangle<float>(r.getRight() - 30, py - 6, 28, 12), juce::Justification::centredRight);
    }
    if (!frame.deltaValid) return;
    juce::Path p;
    for (int i = 0; i < grid::kPoints; ++i) {
        const float px = axis.x(grid::freqAt(i)), py = mid - juce::jlimit(-12.0f, 12.0f, frame.delta[size_t(i)]) * scale;
        if (i == 0) p.startNewSubPath(px, py);
        else p.lineTo(px, py);
    }
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved));
}

void SpectrumView::paintCursor(juce::Graphics& g, float x, bool pinned)
{
    const double hz = axis.hz(x);
    g.setColour(pinned ? amber.withAlpha(0.8f) : text.withAlpha(0.45f));
    g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());

    juce::StringArray lines { hzText(hz) + " Hz  " + grid::noteName(hz) };
    std::vector<juce::Colour> colours { pinned ? amber : text };
    const double gi = juce::jlimit(0.0, double(grid::kPoints - 1), grid::indexOf(hz));
    const int i0 = int(gi), i1 = std::min(i0 + 1, grid::kPoints - 1);
    for (const auto& t : frame.traces) {
        const auto* in = inputs.find(t.id);
        if (!in || !t.hasData) continue;
        const float v = t.line[size_t(i0)] + float(gi - i0) * (t.line[size_t(i1)] - t.line[size_t(i0)]);
        lines.add(in->name + "  " + juce::String(v + kAes17, 1) + " dB");
        colours.push_back(in->colour);
    }
    const float w = 170, h = 16.0f * float(lines.size()) + 8;
    auto box = juce::Rectangle<float>(x + 10, plot.getY() + (pinned ? 34 : 6), w, h);
    if (box.getRight() > plot.getRight()) box.setX(x - 10 - w);
    g.setColour(well.withAlpha(0.95f));
    g.fillRect(box);
    g.setColour(line);
    g.drawRect(box);
    g.setFont(mono(11.0f));
    for (int i = 0; i < lines.size(); ++i) {
        g.setColour(colours[size_t(i)]);
        g.drawText(lines[i], box.reduced(8, 4).withHeight(16).translated(0, 16.0f * float(i)), juce::Justification::centredLeft);
    }
}

void SpectrumView::mouseMove(const juce::MouseEvent& e)
{
    hoverX = e.position.x;
    repaint();
}

void SpectrumView::mouseExit(const juce::MouseEvent&)
{
    hoverX = -1;
    repaint();
}

void SpectrumView::mouseDown(const juce::MouseEvent& e)
{
    if (!plot.contains(e.position)) return;
    // Click pins a cursor; clicking near the pinned one removes it.
    pinnedHz = pinnedHz > 0 && std::abs(axis.x(pinnedHz) - e.position.x) < 6 ? -1.0f : float(axis.hz(e.position.x));
    repaint();
}

} // namespace fs
