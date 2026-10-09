#include "ui/SidePanel.h"

namespace fs {

using namespace theme;

void SidePanel::setData(const Frame& f, const InputList& l, const Spl& s)
{
    frame = f;
    inputs = l;
    spl = s;
    repaint();
}

void SidePanel::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    g.setColour(gridStrong);
    g.drawVerticalLine(0, 0, float(getHeight()));
    auto r = getLocalBounds().toFloat().reduced(14, 10);
    paintSpl(g, r.removeFromTop(268));
    r.removeFromTop(10);
    paintPeaks(g, r);
}

void SidePanel::paintSpl(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(dim);
    g.setFont(sans(11, true));
    const auto* mic = inputs.splMic();
    auto title = r.removeFromTop(18);
    g.drawText("SPL", title, juce::Justification::centredLeft);
    if (mic) {
        g.setColour(mic->colour);
        g.drawText(mic->name, title, juce::Justification::centredRight);
    }
    if (!mic || !frame.splValid) {
        g.setColour(dim);
        g.setFont(sans(12));
        g.drawFittedText(mic ? "Waiting for audio on " + mic->name : "No SPL mic.\nRight-click an input > Use as SPL mic.", r.removeFromTop(48).toNearestInt(),
                         juce::Justification::topLeft, 3);
        return;
    }

    const double off = frame.splOffset;
    const auto& s = frame.spl;
    auto colourFor = [](double v, double limit) { return v > limit ? red : v > limit - 3 ? amber : text; };
    auto big = [&](const juce::String& name, double v, double limit) {
        auto row = r.removeFromTop(54);
        g.setColour(dim);
        g.setFont(sans(11));
        g.drawText(name + "   limit " + juce::String(limit, 0), row.removeFromTop(14), juce::Justification::centredLeft);
        g.setColour(colourFor(v, limit));
        g.setFont(mono(34));
        g.drawText(v > -100 ? juce::String(v, 1) : juce::String("--"), row, juce::Justification::centredLeft);
    };
    const int w15 = SplMeter::kWindows - 1, w1 = 2; // 15 min, 1 min sliding Leq
    big("LAeq 15 min", s.leqWindow[size_t(w15)][1] + off, spl.limitLAeq15);
    big("LCeq 15 min", s.leqWindow[size_t(w15)][2] + off, spl.limitLCeq15);

    auto small = [&](const juce::String& name, double v) {
        auto row = r.removeFromTop(20);
        g.setColour(dim);
        g.setFont(sans(12));
        g.drawText(name, row, juce::Justification::centredLeft);
        g.setColour(text);
        g.setFont(mono(14));
        g.drawText(v > -100 ? juce::String(v, 1) : juce::String("--"), row, juce::Justification::centredRight);
    };
    r.removeFromTop(4);
    small("LA Fast", s.fast[1] + off);
    small("LC Fast", s.fast[2] + off);
    small("LAeq 1 min", s.leqWindow[size_t(w1)][1] + off);
    small("LCpeak (max)", s.cPeak + off);
    small("LA Fast max", s.maxFast[1] + off);

    r.removeFromTop(4);
    g.setFont(sans(11));
    const bool est = spl.calibration == "estimate";
    g.setColour(est ? amber : dim);
    g.drawText(est ? utf8("Not calibrated (estimate) — Settings") : "Calibrated (" + spl.calibration + ")", r.removeFromTop(16), juce::Justification::centredLeft);
    g.setColour(spl.logStatus.isEmpty() ? dim : red);
    g.drawText(spl.logStatus.isEmpty() ? "Logging 1 s / daily CSV" : "Log: " + spl.logStatus, r.removeFromTop(16), juce::Justification::centredLeft);
}

void SidePanel::paintPeaks(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(gridStrong);
    g.drawHorizontalLine(juce::roundToInt(r.getY()), r.getX(), r.getRight());
    r.removeFromTop(8);
    g.setColour(dim);
    g.setFont(sans(11, true));
    g.drawText("FEEDBACK / PEAKS", r.removeFromTop(18), juce::Justification::centredLeft);

    struct Row { Peak p; juce::Colour c; };
    std::vector<Row> rows;
    for (const auto& t : frame.traces)
        if (const auto* in = inputs.find(t.id))
            for (const auto& p : t.peaks) rows.push_back({ p, in->colour });
    std::sort(rows.begin(), rows.end(), [](auto& a, auto& b) { return a.p.prominenceDb > b.p.prominenceDb; });

    if (rows.empty()) {
        g.setColour(dim);
        g.setFont(sans(12));
        g.drawText("Nothing ringing", r.removeFromTop(20), juce::Justification::centredLeft);
        return;
    }
    for (const auto& row : rows) {
        if (r.getHeight() < 40) break;
        auto line = r.removeFromTop(40);
        // Flash for the first 2 s after it is reported: that is when you reach for the EQ.
        const bool fresh = row.p.ageSec < 3.5f && (juce::Time::getMillisecondCounter() / 250) % 2 == 0;
        g.setColour(fresh ? red.withAlpha(0.25f) : panelHi);
        g.fillRoundedRectangle(line.reduced(0, 2), 4);
        auto inner = line.reduced(8, 4);
        g.setColour(row.c);
        g.fillEllipse(inner.removeFromLeft(8).withSizeKeepingCentre(8, 8));
        inner.removeFromLeft(6);
        auto top = inner.removeFromTop(18);
        g.setColour(text);
        g.setFont(mono(15));
        g.drawText(hzText(row.p.hz) + " Hz", top, juce::Justification::centredLeft);
        g.setColour(red);
        g.drawText("+" + juce::String(row.p.prominenceDb, 0) + " dB", top, juce::Justification::centredRight);
        g.setColour(dim);
        g.setFont(mono(11));
        g.drawText(row.p.note + "   " + juce::String(row.p.ageSec, 1) + " s", inner, juce::Justification::centredLeft);
    }
}

} // namespace fs
