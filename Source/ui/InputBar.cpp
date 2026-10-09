#include "ui/InputBar.h"

namespace fs {

using namespace theme;

class InputBar::Chip : public juce::Component, public juce::SettableTooltipClient
{
public:
    Chip(InputBar& b, const Input& i, int n) : bar(b), input(i), number(n)
    {
        setRepaintsOnMouseActivity(true);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip(utf8("Click: focus · Right-click: rename, channel, colour, SPL mic, delta, remove"));
    }

    int preferredWidth() const
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText(sans(13, true), label(), 0, 0);
        return int(ga.getBoundingBox(0, -1, true).getWidth()) + 44;
    }

    void paint(juce::Graphics& g) override
    {
        const bool focused = bar.state.inputs.focusedId == input.id;
        auto r = getLocalBounds().toFloat().reduced(1, 6);
        g.setColour(focused ? panelHi.brighter(0.15f) : (isMouseOver() ? panelHi : panel));
        g.fillRoundedRectangle(r, 5);
        if (focused) {
            g.setColour(input.colour);
            g.drawRoundedRectangle(r, 5, 1.5f);
        }
        auto inner = r.reduced(10, 0);
        g.setColour(input.muted ? dim : input.colour);
        g.fillEllipse(inner.removeFromLeft(10).withSizeKeepingCentre(10, 10));
        inner.removeFromLeft(6);
        const bool live = input.channel < bar.state.channelNames.size();
        g.setColour(input.muted || !live ? dim : text);
        g.setFont(sans(13, true));
        g.drawText(label(), inner, juce::Justification::centredLeft);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) bar.showMenu(input.id);
        else bar.change([id = input.id](InputList& l) { l.focusedId = id; });
    }

private:
    juce::String label() const
    {
        juce::String s = juce::String(number) + "  " + input.name;
        if (input.splMic) s += "  SPL";
        const auto& l = bar.state.inputs;
        if (l.deltaA == input.id) s += "  A";
        if (l.deltaB == input.id) s += "  B";
        if (input.channel >= bar.state.channelNames.size()) s += "  (no ch " + juce::String(input.channel + 1) + ")";
        return s;
    }

    InputBar& bar;
    Input input;
    int number;
};

InputBar::~InputBar() = default;

InputBar::InputBar()
{
    auto style = [this](juce::Button& b, const juce::String& tip) {
        b.setColour(juce::TextButton::buttonColourId, panel);
        b.setColour(juce::TextButton::buttonOnColourId, panelHi.brighter(0.25f));
        b.setColour(juce::TextButton::textColourOffId, dim);
        b.setColour(juce::TextButton::textColourOnId, text);
        b.setTooltip(tip);
        addAndMakeVisible(b);
    };
    style(add, "Add an input (up to 16)");
    add.onClick = [this] { change([this](InputList& l) { if (int id = l.add(state.channelNames.size()); id > 0) l.focusedId = id; }); };

    style(fast, utf8("Fast averaging (≈125 ms) · A cycles"));
    style(slow, utf8("Slow averaging (1 s) · A cycles"));
    style(inf, utf8("Infinite averaging since reset (R) · A cycles"));
    fast.setConnectedEdges(juce::Button::ConnectedOnRight);
    slow.setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    inf.setConnectedEdges(juce::Button::ConnectedOnLeft);
    fast.onClick = [this] { if (onAveraging) onAveraging(Averaging::Fast); };
    slow.onClick = [this] { if (onAveraging) onAveraging(Averaging::Slow); };
    inf.onClick = [this] { if (onAveraging) onAveraging(Averaging::Infinite); };

    for (int r : MtwSpectrum::kResolutions) resolution.addItem("1/" + juce::String(r) + " oct", r);
    resolution.setTooltip("RTA band resolution");
    resolution.onChange = [this] { if (onResolution) onResolution(resolution.getSelectedId()); };
    addAndMakeVisible(resolution);

    style(bars, "RTA bars for the focused input (B)");
    style(delta, utf8("A − B delta trace (D)"));
    style(freeze, "Freeze the display (Space)");
    style(capture, utf8("Capture the focused trace as a reference (C) · right-click: clear (X)"));
    style(settings, utf8("Audio device, calibration, limits (⌘,)"));
    bars.onClick = [this] { if (onBars) onBars(); };
    delta.onClick = [this] { if (onDelta) onDelta(); };
    freeze.onClick = [this] { if (onFreeze) onFreeze(); };
    capture.onClick = [this] {
        if (juce::ModifierKeys::currentModifiers.isPopupMenu()) { if (onClearCaptures) onClearCaptures(); }
        else if (onCapture) onCapture();
    };
    settings.onClick = [this] { if (onSettings) onSettings(); };
}

void InputBar::setState(const State& s)
{
    const bool chipsChanged = !(s.inputs == state.inputs) || s.channelNames != state.channelNames;
    state = s;
    fast.setToggleState(s.averaging == Averaging::Fast, juce::dontSendNotification);
    slow.setToggleState(s.averaging == Averaging::Slow, juce::dontSendNotification);
    inf.setToggleState(s.averaging == Averaging::Infinite, juce::dontSendNotification);
    resolution.setSelectedId(s.bandsPerOctave, juce::dontSendNotification);
    bars.setToggleState(s.bars, juce::dontSendNotification);
    delta.setToggleState(s.delta, juce::dontSendNotification);
    freeze.setToggleState(s.frozen, juce::dontSendNotification);
    freeze.setColour(juce::TextButton::buttonOnColourId, amber.withAlpha(0.6f));
    capture.setButtonText(s.captures > 0 ? "Capture (" + juce::String(s.captures) + ")" : "Capture");
    add.setEnabled(int(s.inputs.inputs.size()) < InputList::kMax);
    if (chipsChanged) rebuildChips();
}

void InputBar::rebuildChips()
{
    chips.clear();
    int n = 1;
    for (const auto& in : state.inputs.inputs) {
        chips.push_back(std::make_unique<Chip>(*this, in, n++));
        addAndMakeVisible(*chips.back());
    }
    resized();
    repaint();
}

void InputBar::paint(juce::Graphics& g)
{
    g.fillAll(bg);
    g.setColour(gridStrong);
    g.drawHorizontalLine(getHeight() - 1, 0, float(getWidth()));
}

void InputBar::resized()
{
    auto r = getLocalBounds().reduced(8, 0);
    auto right = r.removeFromRight(572);
    auto place = [&](juce::Component& c, int w) { c.setBounds(right.removeFromLeft(w).reduced(2, 8)); };
    place(fast, 50);
    place(slow, 50);
    place(inf, 44);
    right.removeFromLeft(8);
    place(resolution, 92);
    right.removeFromLeft(8);
    place(bars, 50);
    place(delta, 54);
    place(freeze, 60);
    place(capture, 88);
    place(settings, 76);

    for (auto& c : chips) {
        const int w = std::min(c->preferredWidth(), std::max(60, r.getWidth() - 40));
        c->setBounds(r.removeFromLeft(w));
        r.removeFromLeft(4);
    }
    add.setBounds(r.removeFromLeft(36).reduced(2, 8));
}

void InputBar::change(const std::function<void(InputList&)>& edit)
{
    auto l = state.inputs;
    edit(l);
    if (onInputs) onInputs(l);
}

void InputBar::showMenu(int id)
{
    const auto* in = state.inputs.find(id);
    if (!in) return;
    juce::PopupMenu m, channels, colours;
    for (int ch = 0; ch < std::max(state.channelNames.size(), in->channel + 1); ++ch) {
        const auto name = ch < state.channelNames.size() ? state.channelNames[ch] : juce::String("(not on this device)");
        channels.addItem(1000 + ch, juce::String(ch + 1) + "  " + name, true, ch == in->channel);
    }
    for (int c = 0; c < Palette::kDistinctHues; ++c)
        colours.addColouredItem(2000 + c, "Colour " + juce::String(c + 1), Palette::colour(c), true, Palette::colour(c) == in->colour);
    colours.addSeparator();
    colours.addItem(2999, "Next new colour");

    m.addSectionHeader(in->name);
    m.addItem(1, utf8("Rename…"));
    m.addSubMenu("Channel", channels);
    m.addSubMenu("Colour", colours);
    m.addItem(2, in->muted ? "Show / analyse" : "Hide (stop analysing)");
    m.addSeparator();
    m.addItem(3, "Use as SPL mic", !in->muted, in->splMic);
    m.addItem(4, "Delta A (mic)", true, state.inputs.deltaA == id);
    m.addItem(5, "Delta B (reference)", true, state.inputs.deltaB == id);
    m.addSeparator();
    m.addItem(6, "Remove", state.inputs.inputs.size() > 1);

    m.showMenuAsync(juce::PopupMenu::Options(), [this, id](int r) {
        if (r == 0) return;
        if (r == 1) { rename(id); return; }
        change([r, id](InputList& l) {
            auto* i = l.find(id);
            if (!i) return;
            if (r >= 1000 && r < 2000) i->channel = r - 1000;
            else if (r >= 2000 && r < 2999) i->colour = Palette::colour(r - 2000);
            else if (r == 2999) i->colour = Palette::colour(l.nextColour++);
            else if (r == 2) i->muted = !i->muted;
            else if (r == 3) l.setSplMic(i->splMic ? -1 : id);
            else if (r == 4) { l.deltaA = l.deltaA == id ? -1 : id; if (l.deltaB == id) l.deltaB = -1; }
            else if (r == 5) { l.deltaB = l.deltaB == id ? -1 : id; if (l.deltaA == id) l.deltaA = -1; }
            else if (r == 6) l.remove(id);
        });
    });
}

void InputBar::rename(int id)
{
    const auto* in = state.inputs.find(id);
    if (!in) return;
    auto* w = new juce::AlertWindow("Rename input", "", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", in->name);
    w->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<InputBar> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w, id](int r) {
        const auto name = w->getTextEditorContents("name").trim();
        if (r == 1 && safe && name.isNotEmpty())
            safe->change([id, name](InputList& l) { if (auto* i = l.find(id)) i->name = name.substring(0, 32); });
    }), true);
}

} // namespace fs
