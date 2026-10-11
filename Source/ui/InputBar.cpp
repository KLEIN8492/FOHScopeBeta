#include "ui/InputBar.h"

namespace fs {

using namespace theme;

// One input: square swatch, number, name, state tags. Focused = its colour underlines it.
class InputBar::Chip : public juce::Component, public juce::SettableTooltipClient
{
public:
    Chip(InputBar& b, const Input& i, int n) : bar(b), input(i), number(n)
    {
        setRepaintsOnMouseActivity(true);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("Click: focus. Right-click: rename, channel, colour, SPL mic, delta, remove. Full routing: " + shortcut(","));
    }

    int preferredWidth() const
    {
        int w = 10 + 10 + 6 + juce::roundToInt(juce::GlyphArrangement::getStringWidth(mono(11), label())) + 10;
        for (auto& t : tags()) w += juce::roundToInt(juce::GlyphArrangement::getStringWidth(mono(9.5f), t)) + 10;
        return w;
    }

    void paint(juce::Graphics& g) override
    {
        const bool focused = bar.state.inputs.focusedId == input.id;
        const bool live = bar.isLive(input.port);
        auto r = getLocalBounds();
        if (focused || isMouseOver()) {
            g.setColour(well);
            g.fillRect(r);
        }
        if (focused) {
            g.setColour(input.colour);
            g.fillRect(r.removeFromBottom(2));
        }
        g.setColour(line);
        g.drawVerticalLine(getWidth() - 1, 0, float(getHeight()));

        auto inner = getLocalBounds().reduced(10, 0);
        swatch(g, inner.removeFromLeft(10).withSizeKeepingCentre(10, 10), input.muted ? dim : input.colour);
        inner.removeFromLeft(6);
        g.setColour(input.muted || !live ? dim : (focused ? text : text.withAlpha(0.8f)));
        g.setFont(mono(11));
        const auto l = label();
        g.drawText(l, inner.removeFromLeft(juce::roundToInt(juce::GlyphArrangement::getStringWidth(mono(11), l)) + 2), juce::Justification::centredLeft, false);
        for (auto& t : tags()) {
            inner.removeFromLeft(4);
            const int w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(mono(9.5f), t)) + 6;
            const auto tag = inner.removeFromLeft(w).withSizeKeepingCentre(w, 14);
            const auto c = t == "OFFLINE" || t == "NOT PATCHED" ? red : (t == "SPL" ? amber : accent);
            g.setColour(c);
            g.drawRect(tag);
            g.setFont(mono(9.5f));
            g.drawText(t, tag, juce::Justification::centred);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) bar.showMenu(input.id);
        else bar.change([id = input.id](InputList& l) { l.focusedId = id; });
    }

private:
    juce::String label() const { return juce::String(number) + " " + input.name.toUpperCase(); }

    juce::StringArray tags() const
    {
        juce::StringArray t;
        if (input.splMic) t.add("SPL");
        const auto& l = bar.state.inputs;
        if (l.deltaA == input.id) t.add("A");
        if (l.deltaB == input.id) t.add("B");
        if (!input.port.valid()) t.add("NOT PATCHED");
        else if (!bar.isLive(input.port)) t.add("OFFLINE");
        return t;
    }

    InputBar& bar;
    Input input;
    int number;
};

InputBar::~InputBar() = default;

InputBar::InputBar()
{
    auto setup = [this](juce::Button& b, const juce::String& tip) {
        b.setTooltip(tip);
        addAndMakeVisible(b);
    };
    setup(add, "Add an input: opens Inputs & Routing (up to 16)");
    add.onClick = [this] { if (onAdd) onAdd(); };

    setup(fast, utf8("Fast averaging (≈125 ms). A cycles"));
    setup(slow, "Slow averaging (1 s). A cycles");
    setup(inf, "Infinite averaging since reset (R). A cycles");
    fast.onClick = [this] { if (onAveraging) onAveraging(Averaging::Fast); };
    slow.onClick = [this] { if (onAveraging) onAveraging(Averaging::Slow); };
    inf.onClick = [this] { if (onAveraging) onAveraging(Averaging::Infinite); };

    for (int r : MtwSpectrum::kResolutions) resolution.addItem("1/" + juce::String(r), r);
    resolution.setTooltip("RTA band resolution (octave)");
    resolution.onChange = [this] { if (onResolution) onResolution(resolution.getSelectedId()); };
    addAndMakeVisible(resolution);

    setup(autoScale, "dB scale follows the material (Z: fit once). Wheel: pan, cmd+wheel / pinch: zoom, drag the dB labels: pan");
    autoScale.onClick = [this] { if (onAutoScale) onAutoScale(); };
    setup(bars, "RTA bars for the focused input (B)");
    setup(delta, utf8("A − B delta trace (D)"));
    setup(freeze, "Freeze the display (Space)");
    setup(capture, "Capture the focused trace as a reference (C). Right-click: clear (X)");
    setup(settings, "Inputs & routing, audio, calibration, limits (" + shortcut(",") + ")");
    bars.onClick = [this] { if (onBars) onBars(); };
    delta.onClick = [this] { if (onDelta) onDelta(); };
    freeze.onClick = [this] { if (onFreeze) onFreeze(); };
    capture.onClick = [this] {
        if (juce::ModifierKeys::currentModifiers.isPopupMenu()) { if (onClearCaptures) onClearCaptures(); }
        else if (onCapture) onCapture();
    };
    settings.onClick = [this] { if (onSettings) onSettings(); };
    freeze.setColour(juce::TextButton::buttonOnColourId, amber);
}

void InputBar::setState(const State& s)
{
    auto statesOf = [](const State& st) {
        juce::StringArray k;
        for (auto& i : st.interfaces) k.add(i.name + "/" + juce::String(int(i.state)) + "/" + juce::String(i.inputs.size()));
        return k;
    };
    const bool chipsChanged = !(s.inputs == state.inputs) || statesOf(s) != statesOf(state);
    state = s;
    fast.setToggleState(s.averaging == Averaging::Fast, juce::dontSendNotification);
    slow.setToggleState(s.averaging == Averaging::Slow, juce::dontSendNotification);
    inf.setToggleState(s.averaging == Averaging::Infinite, juce::dontSendNotification);
    resolution.setSelectedId(s.bandsPerOctave, juce::dontSendNotification);
    bars.setToggleState(s.bars, juce::dontSendNotification);
    autoScale.setToggleState(s.autoScale, juce::dontSendNotification);
    delta.setToggleState(s.delta, juce::dontSendNotification);
    freeze.setToggleState(s.frozen, juce::dontSendNotification);
    capture.setButtonText(s.captures > 0 ? "Cap " + juce::String(s.captures) : "Cap");
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
    g.fillAll(panel);
    g.setColour(line);
    g.drawHorizontalLine(getHeight() - 1, 0, float(getWidth()));
    if (!chips.empty()) g.drawVerticalLine(0, 0, float(getHeight()));
    caption(g, "AVG", { fast.getX() - 34, 0, 30, getHeight() }, juce::Justification::centredRight);
    caption(g, "RTA", { resolution.getX() - 34, 0, 30, getHeight() }, juce::Justification::centredRight);
}

void InputBar::resized()
{
    auto r = getLocalBounds().withTrimmedBottom(1);
    auto right = r.removeFromRight(646).reduced(6, 5);
    auto place = [&](juce::Component& c, int w, int gapAfter = 0) {
        c.setBounds(right.removeFromLeft(w));
        right.removeFromLeft(gapAfter);
    };
    right.removeFromLeft(34);
    place(fast, 44, -1); // shared borders: a segmented control
    place(slow, 44, -1);
    place(inf, 40, 44);
    place(resolution, 60, 4);
    place(autoScale, 50, 10);
    place(bars, 46, 4);
    place(delta, 52, 4);
    place(freeze, 56, 4);
    place(capture, 52, 4);
    place(settings, 60);
    settings.setButtonText("Setup");

    for (auto& c : chips) {
        const int w = std::min(c->preferredWidth(), std::max(60, r.getWidth() - 40));
        c->setBounds(r.removeFromLeft(w));
    }
    add.setBounds(r.removeFromLeft(34).reduced(5, 5));
}

const DeviceHub::Info* InputBar::interfaceInfo(const juce::String& name) const
{
    for (auto& i : state.interfaces)
        if (i.name == name) return &i;
    return nullptr;
}

bool InputBar::isLive(const PortRef& p) const
{
    const auto* i = p.valid() ? interfaceInfo(p.device) : nullptr;
    return i && i->state == DeviceHub::State::open && p.channel < i->inputs.size();
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
    // Channel: interface > channel. Item ids 100000 + index into `ports`.
    juce::PopupMenu m, channels, colours;
    std::vector<PortRef> ports;
    for (const auto& iface : state.interfaces) {
        juce::PopupMenu sub;
        for (int ch = 0; ch < iface.inputs.size(); ++ch) {
            ports.push_back({ iface.name, ch });
            sub.addItem(100000 + int(ports.size()) - 1, juce::String(ch + 1) + "  " + iface.inputs[ch], true, in->port == ports.back());
        }
        channels.addSubMenu(iface.name, sub, true, nullptr, in->port.device == iface.name);
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
    m.addItem(6, "Remove");

    m.showMenuAsync(juce::PopupMenu::Options(), [this, id, ports](int r) {
        if (r == 0) return;
        if (r == 1) { rename(id); return; }
        change([r, id, ports](InputList& l) {
            auto* i = l.find(id);
            if (!i) return;
            if (r >= 100000 && r - 100000 < int(ports.size())) i->port = ports[size_t(r - 100000)];
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
