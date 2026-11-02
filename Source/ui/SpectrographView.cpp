#include "ui/SpectrographView.h"

namespace fs {

using namespace theme;

namespace {
constexpr int kRowsPerSecond = SpectrogramHistory::kRowsPerSecond;
}

SpectrographView::SpectrographView()
{
    setOpaque(true);
    live.setTooltip("Back to live (End)");
    live.onClick = [this] { goLive(); };
    addChildComponent(live);
    setTooltip("Scroll to go back in time (15 min kept). Hover reads frequency, level and age.");
}

void SpectrographView::configure(int s, const Units& u)
{
    bool redraw = false;
    if (!(u == units)) {
        units = u;
        units.top = std::max(u.top, u.floor + 10);
        rebuildColours();
        redraw = true; // history holds levels: the whole view takes the new colours
    }
    if (s != seconds || !image.isValid()) {
        seconds = s;
        rows = seconds * kRowsPerSecond;
        image = juce::Image(juce::Image::RGB, grid::kPoints, rows, true);
        redraw = true;
    }
    if (redraw) redrawAll();
}

void SpectrographView::rebuildColours()
{
    const auto& lut = heatMap();
    for (int q = 0; q < 256; ++q) {
        const float level = SpectrogramHistory::dequantize(uint8_t(q)) + (units.spl ? float(units.splOffset) : kAes17);
        const float t = (level - units.floor) / (units.top - units.floor);
        colourOf[size_t(q)] = lut[size_t(juce::jlimit(0, 255, int(t * 255.0f)))];
    }
}

void SpectrographView::clear()
{
    history.clear();
    parkedEnd.reset();
    live.setVisible(false);
    redrawAll();
}

void SpectrographView::drawRow(int imageRow, const uint8_t* cells)
{
    juce::Image::BitmapData px(image, 0, imageRow, grid::kPoints, 1, juce::Image::BitmapData::writeOnly);
    for (int i = 0; i < grid::kPoints; ++i) px.setPixelColour(i, 0, cells ? juce::Colour(colourOf[cells[i]]) : juce::Colours::black);
}

void SpectrographView::addColumn(const float* col)
{
    history.push(col); // always recorded, also while parked or frozen
    if (!image.isValid() || parkedEnd) return;
    head = (head + rows - 1) % rows; // newest row moves up
    drawRow(head, history.row(0));
}

uint64_t SpectrographView::viewEnd() const
{
    return parkedEnd ? *parkedEnd : (history.pushed() == 0 ? 0 : history.pushed() - 1);
}

void SpectrographView::redrawAll()
{
    if (!image.isValid()) return;
    head = 0;
    const int back = int(history.rowsAgo(viewEnd()));
    for (int r = 0; r < rows; ++r) drawRow(r, history.row(back + r));
    repaint();
}

void SpectrographView::park(uint64_t end)
{
    end = history.clampEnd(end, rows);
    if (history.isLive(end)) {
        goLive();
        return;
    }
    if (parkedEnd && *parkedEnd == end) return;
    parkedEnd = end;
    live.setVisible(true);
    redrawAll();
}

void SpectrographView::goLive()
{
    if (!parkedEnd) return;
    parkedEnd.reset();
    live.setVisible(false);
    redrawAll();
}

void SpectrographView::setFrozen(bool frozen)
{
    if (frozen && !parkedEnd && history.pushed() > 0) {
        parkedEnd = history.pushed() - 1; // hold this moment; recording continues underneath
        live.setVisible(true);
        repaint();
    } else if (!frozen) {
        goLive();
    }
}

// Wheel up / trackpad swipe = further back, ~10 % of the visible span per notch.
void SpectrographView::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (history.pushed() == 0 || rows == 0) return;
    const auto shift = juce::roundToInt(w.deltaY * float(rows));
    if (shift == 0) return;
    const auto end = int64_t(viewEnd()) - shift;
    park(uint64_t(std::max<int64_t>(0, end)));
}

void SpectrographView::mouseMove(const juce::MouseEvent& e)
{
    hover = e.position;
    repaint();
}

void SpectrographView::mouseExit(const juce::MouseEvent&)
{
    hover = { -1, -1 };
    repaint();
}

juce::Rectangle<float> SpectrographView::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft(float(kPlotLeft)).withTrimmedRight(float(kPlotRight)).withTrimmedBottom(4);
}

void SpectrographView::resized()
{
    const auto plot = plotArea();
    live.setBounds(juce::roundToInt(plot.getX()) + 212, juce::roundToInt(plot.getY()) + 6, 60, 18);
}

// "now", "-12s", "-2:35".
juce::String SpectrographView::ago(double s)
{
    const int t = juce::roundToInt(s);
    if (t == 0) return "now";
    if (t < 60) return "-" + juce::String(t) + "s";
    return "-" + juce::String(t / 60) + ":" + juce::String(t % 60).paddedLeft('0', 2);
}

void SpectrographView::paint(juce::Graphics& g)
{
    g.fillAll(bg);
    const auto plot = plotArea();
    const FreqAxis axis { plot.getX(), plot.getRight() };
    if (!image.isValid() || rows == 0) return;

    // Grid point i covers [i − ½, i + ½]; stretch the image so its columns land on the axis.
    const float x0 = axis.x(grid::freqAt(-0.5)), x1 = axis.x(grid::freqAt(grid::kPoints - 0.5));
    g.saveState();
    g.reduceClipRegion(plot.toNearestInt());
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    // Rows head..end are the newest; draw them on top, then the wrapped older part.
    const float rowH = plot.getHeight() / float(rows);
    const int newer = rows - head;
    const auto stretch = juce::RectanglePlacement::stretchToFit;
    g.drawImage(image.getClippedImage({ 0, head, grid::kPoints, newer }), { x0, plot.getY(), x1 - x0, rowH * float(newer) }, stretch);
    if (head > 0) g.drawImage(image.getClippedImage({ 0, 0, grid::kPoints, head }), { x0, plot.getY() + rowH * float(newer), x1 - x0, rowH * float(head) }, stretch);
    g.restoreState();

    // Time axis: age of each row; when parked, counted from now (so -2:35 means 2 min 35 s ago).
    const double back = double(history.rowsAgo(viewEnd())) / kRowsPerSecond;
    g.setFont(mono(10.5f));
    for (int s = 0; s <= seconds; s += seconds >= 30 ? 10 : 5) {
        const float py = plot.getY() + plot.getHeight() * float(s) / float(seconds);
        g.setColour(gridStrong);
        g.drawHorizontalLine(juce::roundToInt(py), plot.getX() - 4, plot.getX());
        g.setColour(parkedEnd ? amber : dim);
        g.drawText(ago(back + s), juce::Rectangle<float>(0, py - (s == 0 ? 0 : 7), float(kPlotLeft) - 6, 14), juce::Justification::centredRight);
    }
    if (label.isNotEmpty()) {
        g.setColour(well.withAlpha(0.85f));
        auto r = juce::Rectangle<float>(plot.getX() + 6, plot.getY() + 6, 200, 18);
        g.fillRect(r);
        g.setColour(labelColour);
        g.setFont(mono(10.5f, true));
        g.drawText(label.toUpperCase(), r.reduced(6, 0), juce::Justification::centredLeft);
    }
    paintLegend(g, plot);
    paintHover(g, plot);
}

// Hover readout: frequency, note, level in the current units, and how long ago.
void SpectrographView::paintHover(juce::Graphics& g, juce::Rectangle<float> plot)
{
    if (!plot.contains(hover) || rows == 0) return;
    const FreqAxis axis { plot.getX(), plot.getRight() };
    const double hz = axis.hz(hover.x);
    const int i = juce::jlimit(0, grid::kPoints - 1, juce::roundToInt(grid::indexOf(hz)));
    const int r = juce::jlimit(0, rows - 1, int((hover.y - plot.getY()) / plot.getHeight() * float(rows)));
    const int rowsAgo = int(history.rowsAgo(viewEnd())) + r;
    const float level = history.level(rowsAgo, i);
    juce::String readout = hzText(hz) + " Hz  " + grid::noteName(hz) + "   ";
    readout += level > -199.0f ? juce::String(level + (units.spl ? float(units.splOffset) : kAes17), 1) + (units.spl ? " dB SPL" : " dBFS")
                               : juce::String("--");
    readout += "   " + ago(double(rowsAgo) / kRowsPerSecond);

    g.setColour(text.withAlpha(0.45f));
    g.drawVerticalLine(juce::roundToInt(hover.x), plot.getY(), plot.getBottom());
    g.drawHorizontalLine(juce::roundToInt(hover.y), plot.getX(), plot.getRight());
    g.setFont(mono(11));
    const float w = juce::GlyphArrangement::getStringWidth(mono(11), readout) + 16;
    auto box = juce::Rectangle<float>(hover.x + 12, hover.y + 10, w, 20);
    if (box.getRight() > plot.getRight()) box.setX(hover.x - 12 - w);
    if (box.getBottom() > plot.getBottom()) box.setY(hover.y - 30);
    g.setColour(well.withAlpha(0.95f));
    g.fillRect(box);
    g.setColour(line);
    g.drawRect(box);
    g.setColour(text);
    g.drawText(readout, box.reduced(8, 0), juce::Justification::centredLeft);
}

// Colour legend, top-right inside the plot: the heat ramp with its floor / top and units.
void SpectrographView::paintLegend(juce::Graphics& g, juce::Rectangle<float> plot)
{
    const auto unit = units.spl ? juce::String(units.estimate ? "dB SPL (est.)" : "dB SPL") : juce::String("dBFS");
    auto box = juce::Rectangle<float>(plot.getRight() - 236, plot.getY() + 6, 230, 30);
    g.setColour(well.withAlpha(0.9f));
    g.fillRect(box);
    g.setColour(line);
    g.drawRect(box);
    auto inner = box.reduced(6, 4);
    auto ramp = inner.removeFromTop(8);
    const auto& lut = heatMap();
    for (int x = 0; x < int(ramp.getWidth()); ++x) {
        g.setColour(juce::Colour(lut[size_t(juce::jlimit(0, 255, int(255.0f * float(x) / ramp.getWidth())))]));
        g.fillRect(ramp.getX() + float(x), ramp.getY(), 1.0f, ramp.getHeight());
    }
    g.setFont(mono(9.5f));
    g.setColour(text);
    g.drawText(juce::String(juce::roundToInt(units.floor)), inner, juce::Justification::bottomLeft);
    g.drawText(juce::String(juce::roundToInt(units.top)), inner, juce::Justification::bottomRight);
    g.setColour(units.spl && units.estimate ? amber : dim);
    g.drawText(unit, inner, juce::Justification::centredBottom);
}

} // namespace fs
