#include "ui/SpectrographView.h"

namespace fs {

using namespace theme;

void SpectrographView::configure(int s, float floorDbfs, float topDbfs)
{
    floorDb = floorDbfs;
    topDb = std::max(topDbfs, floorDbfs + 10);
    if (s == seconds && image.isValid()) return;
    seconds = s;
    rows = seconds * kRowsPerSecond;
    image = juce::Image(juce::Image::RGB, grid::kPoints, rows, true);
    head = 0;
    repaint();
}

void SpectrographView::clear()
{
    if (image.isValid()) image.clear(image.getBounds());
    repaint();
}

void SpectrographView::addColumn(const float* col)
{
    if (!image.isValid()) return;
    head = (head + rows - 1) % rows; // newest row moves up
    const auto& lut = heatMap();
    juce::Image::BitmapData px(image, 0, head, grid::kPoints, 1, juce::Image::BitmapData::writeOnly);
    for (int i = 0; i < grid::kPoints; ++i) {
        const float t = (col[i] + kAes17 - floorDb) / (topDb - floorDb);
        const auto c = lut[size_t(juce::jlimit(0, 255, int(t * 255.0f)))];
        px.setPixelColour(i, 0, juce::Colour(c));
    }
}

void SpectrographView::paint(juce::Graphics& g)
{
    g.fillAll(bg);
    const auto plot = getLocalBounds().toFloat().withTrimmedLeft(float(kPlotLeft)).withTrimmedRight(float(kPlotRight)).withTrimmedBottom(4);
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

    g.setFont(mono(10.5f));
    for (int s = 0; s <= seconds; s += seconds >= 30 ? 10 : 5) {
        const float py = plot.getY() + plot.getHeight() * float(s) / float(seconds);
        g.setColour(gridStrong);
        g.drawHorizontalLine(juce::roundToInt(py), plot.getX() - 4, plot.getX());
        g.setColour(dim);
        g.drawText(s == 0 ? "now" : "-" + juce::String(s) + "s", juce::Rectangle<float>(0, py - (s == 0 ? 0 : 7), float(kPlotLeft) - 6, 14),
                   juce::Justification::centredRight);
    }
    if (label.isNotEmpty()) {
        g.setColour(well.withAlpha(0.85f));
        auto r = juce::Rectangle<float>(plot.getX() + 6, plot.getY() + 6, 200, 18);
        g.fillRect(r);
        g.setColour(labelColour);
        g.setFont(mono(10.5f, true));
        g.drawText(label.toUpperCase(), r.reduced(6, 0), juce::Justification::centredLeft);
    }
}

} // namespace fs
