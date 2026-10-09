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
    g.setColour(line);
    g.drawVerticalLine(0, 0, float(getHeight()));
    auto r = getLocalBounds().reduced(10, 6).withTrimmedLeft(1);
    paintSpl(g, r.removeFromTop(250));
    paintPeaks(g, r);
}

void SidePanel::paintSpl(juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto* mic = inputs.splMic();
    auto title = r.removeFromTop(20);
    caption(g, "SPL", title);
    if (mic) {
        g.setColour(mic->colour);
        g.setFont(mono(10, true));
        g.drawText(mic->name.toUpperCase(), title, juce::Justification::centredRight);
    }
    if (!mic || !frame.splValid) {
        g.setColour(dim);
        g.setFont(mono(11));
        g.drawFittedText(mic ? "waiting for audio on " + mic->name : "no SPL mic.\nSettings > Inputs & Routing > SPL", r.removeFromTop(40),
                         juce::Justification::topLeft, 3);
        return;
    }

    const double off = frame.splOffset;
    const auto& s = frame.spl;
    // VenueTuner limit convention: plain below, amber within 3 dB, white on red above.
    // limit <= 0: no limit (plain value).
    auto big = [&](const juce::String& name, double v, double limit) {
        auto row = r.removeFromTop(56);
        auto head = row.removeFromTop(16);
        caption(g, name, head);
        if (limit > 0) {
            g.setColour(dim);
            g.setFont(mono(10));
            g.drawText("LIM " + juce::String(limit, 0), head, juce::Justification::centredRight);
        }
        const bool over = limit > 0 && v > limit, near = limit > 0 && v > limit - 3;
        auto value = row.reduced(0, 2);
        if (over) {
            g.setColour(red);
            g.fillRect(value);
        }
        g.setColour(over ? juce::Colours::white : near ? amber : text);
        g.setFont(mono(30));
        g.drawText(v > -100 ? juce::String(v, 1) : juce::String("--"), value.reduced(over ? 6 : 0, 0), juce::Justification::centredLeft);
    };
    const int w15 = SplMeter::kWindows - 1, w1 = 2; // 15 min, 1 min sliding Leq
    big("LA Fast  dB SPL", s.fast[1] + off, 0);

    // limit > 0: VenueTuner limit colours on the value (amber within 3 dB, white on red over).
    auto small = [&](const juce::String& name, double v, double limit = 0) {
        auto row = r.removeFromTop(18);
        g.setColour(dim);
        g.setFont(mono(11));
        g.drawText(limit > 0 ? name + "  LIM " + juce::String(limit, 0) : name, row, juce::Justification::centredLeft);
        const bool over = limit > 0 && v > limit, near = limit > 0 && v > limit - 3;
        auto value = row.removeFromRight(54);
        if (over) {
            g.setColour(red);
            g.fillRect(value);
        }
        g.setColour(over ? juce::Colours::white : near ? amber : text);
        g.drawText(v > -100 ? juce::String(v, 1) : juce::String("--"), value.withTrimmedRight(over ? 4 : 0), juce::Justification::centredRight);
    };
    r.removeFromTop(4);
    small("LAeq 15 MIN", s.leqWindow[size_t(w15)][1] + off, spl.limitLAeq15);
    small("LCeq 15 MIN", s.leqWindow[size_t(w15)][2] + off, spl.limitLCeq15);
    r.removeFromTop(6);
    small("LC FAST", s.fast[2] + off);
    small("LAeq 1 MIN", s.leqWindow[size_t(w1)][1] + off);
    small("LCpeak MAX", s.cPeak + off);
    small("LA FAST MAX", s.maxFast[1] + off);

    r.removeFromTop(4);
    g.setFont(mono(10));
    const bool est = spl.calibration == "estimate";
    g.setColour(est ? amber : dim);
    g.drawText(est ? "NOT CALIBRATED (ESTIMATE)" : "CAL: " + spl.calibration.toUpperCase(), r.removeFromTop(14), juce::Justification::centredLeft);
}

void SidePanel::paintPeaks(juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour(line);
    g.drawHorizontalLine(r.getY(), float(r.getX() - 10), float(r.getRight() + 10));
    r.removeFromTop(6);
    caption(g, "Feedback / peaks", r.removeFromTop(20));

    struct Row { Peak p; juce::Colour c; };
    std::vector<Row> rows;
    for (const auto& t : frame.traces)
        if (const auto* in = inputs.find(t.id))
            for (const auto& p : t.peaks) rows.push_back({ p, in->colour });
    std::sort(rows.begin(), rows.end(), [](auto& a, auto& b) { return a.p.prominenceDb > b.p.prominenceDb; });

    if (rows.empty()) {
        g.setColour(dim);
        g.setFont(mono(11));
        g.drawText("-- nothing ringing", r.removeFromTop(18), juce::Justification::centredLeft);
        return;
    }
    // Rows blink for the first 2 s after they are reported: that is when you reach for the EQ.
    const bool blink = (juce::Time::getMillisecondCounter() / 250) % 2 == 0;
    for (const auto& row : rows) {
        if (r.getHeight() < 36) break;
        auto line1 = r.removeFromTop(36);
        if (row.p.ageSec < 3.5f && blink) {
            g.setColour(red.withAlpha(0.22f));
            g.fillRect(line1);
        }
        g.setColour(line);
        g.drawHorizontalLine(line1.getBottom() - 1, float(line1.getX()), float(line1.getRight()));
        auto inner = line1.reduced(0, 3);
        swatch(g, inner.removeFromLeft(8).withSizeKeepingCentre(8, 8).translated(0, -7), row.c);
        inner.removeFromLeft(6);
        auto top = inner.removeFromTop(16);
        g.setColour(text);
        g.setFont(mono(13, true));
        g.drawText(hzText(row.p.hz) + " Hz", top, juce::Justification::centredLeft);
        g.setColour(red);
        g.drawText("+" + juce::String(row.p.prominenceDb, 0) + " dB", top, juce::Justification::centredRight);
        g.setColour(dim);
        g.setFont(mono(10.5f));
        g.drawText(row.p.note + "   " + juce::String(row.p.ageSec, 1) + " s", inner, juce::Justification::centredLeft);
    }
}

} // namespace fs
