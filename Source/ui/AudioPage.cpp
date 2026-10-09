#include "ui/AudioPage.h"

namespace fs {

using namespace theme;

namespace {
constexpr int kPad = 16, kRowH = 22, kListTop = 120;
constexpr int kRates[] = { 44100, 48000, 88200, 96000 };
constexpr int kBuffers[] = { 32, 64, 128, 256, 512, 1024, 2048 };
}

AudioPage::AudioPage(DeviceHub& h, Hooks hk) : hub(h), hooks(std::move(hk))
{
    for (int r : kRates) rate.addItem(juce::String(r / 1000.0, 1) + " kHz", r);
    for (int b : kBuffers) buffer.addItem(juce::String(b) + " samples", b);
    rate.onChange = [this] {
        auto s = hooks.get();
        s.sampleRate = rate.getSelectedId();
        hooks.set(s);
    };
    buffer.onChange = [this] {
        auto s = hooks.get();
        s.bufferSize = buffer.getSelectedId();
        hooks.set(s);
    };
    addAndMakeVisible(rate);
    addAndMakeVisible(buffer);
    refresh();
}

void AudioPage::refresh()
{
    const auto& s = hooks.get();
    rate.setSelectedId(juce::roundToInt(s.sampleRate), juce::dontSendNotification);
    buffer.setSelectedId(s.bufferSize, juce::dontSendNotification);
    interfaces = hub.snapshot();
    repaint();
}

void AudioPage::resized()
{
    rate.setBounds(190, 40, 140, 24);
    buffer.setBounds(190, 72, 140, 24);
}

void AudioPage::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    caption(g, "Format  (every interface opens at this rate and buffer; no resampling)", { kPad, 10, getWidth() - 2 * kPad, 20 });
    g.setFont(mono(11));
    g.setColour(dim);
    g.drawText("Sample rate", kPad, 40, 160, 24, juce::Justification::centredLeft);
    g.drawText("Buffer size", kPad, 72, 160, 24, juce::Justification::centredLeft);

    caption(g, "Interfaces  (open while an input uses them, or while Inputs & Routing is shown)", { kPad, kListTop - 14, getWidth() - 2 * kPad, 14 });
    auto r = juce::Rectangle<int>(kPad, kListTop + 6, getWidth() - 2 * kPad, kRowH);
    const int wName = 280, wState = 90, wCh = 60;
    caption(g, "Name", r.withWidth(wName));
    caption(g, "State", r.withX(r.getX() + wName).withWidth(wState));
    caption(g, "In", r.withX(r.getX() + wName + wState).withWidth(wCh));
    caption(g, "Restarts / error", r.withTrimmedLeft(wName + wState + wCh));
    g.setColour(line);
    g.drawHorizontalLine(r.getBottom(), float(kPad), float(getWidth() - kPad));

    for (const auto& i : interfaces) {
        r.translate(0, kRowH);
        g.setFont(mono(11));
        g.setColour(i.wanted ? text : dim);
        g.drawText(i.name, r.withWidth(wName - 8), juce::Justification::centredLeft, true);
        juce::String state;
        juce::Colour c = dim;
        switch (i.state) {
            case DeviceHub::State::open: state = "OPEN"; c = ok; break;
            case DeviceHub::State::opening: state = "OPENING"; c = amber; break;
            case DeviceHub::State::error: state = "ERROR"; c = red; break;
            case DeviceHub::State::missing: state = "MISSING"; c = red; break;
            case DeviceHub::State::closed: state = "-"; break;
        }
        g.setColour(c);
        g.drawText(state, r.withX(r.getX() + wName).withWidth(wState), juce::Justification::centredLeft);
        g.setColour(dim);
        g.drawText(juce::String(i.inputs.size()), r.withX(r.getX() + wName + wState).withWidth(wCh), juce::Justification::centredLeft);
        juce::String extra = i.restarts > 0 ? juce::String(i.restarts) + " restarts  " : juce::String();
        if (i.error.isNotEmpty()) extra += i.error;
        g.setColour(i.error.isNotEmpty() ? red : dim);
        g.drawText(extra, r.withTrimmedLeft(wName + wState + wCh), juce::Justification::centredLeft, true);
        g.setColour(line);
        g.drawHorizontalLine(r.getBottom(), float(kPad), float(getWidth() - kPad));
    }
    if (interfaces.empty()) {
        g.setColour(amber);
        g.drawText("scanning interfaces...", r.translated(0, kRowH), juce::Justification::centredLeft);
    }
}

} // namespace fs
