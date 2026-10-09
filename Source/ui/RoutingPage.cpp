#include "ui/RoutingPage.h"
#include <set>

namespace fs {

using namespace theme;

namespace {
constexpr int kRowH = 34, kPad = 12;
constexpr float kFloorDb = -60.0f;

// Row columns (x positions).
constexpr int xSwatch = kPad, xName = kPad + 22, wName = 150, xIface = xName + wName + 8, wIface = 230,
              xChan = xIface + wIface + 10, wChan = 150, xMeter = xChan + wChan + 6, wMeter = 76, xTags = xMeter + wMeter + 12;

juce::String stateText(const DeviceHub::Info& i)
{
    switch (i.state) {
        case DeviceHub::State::open: return "open";
        case DeviceHub::State::opening: return "opening...";
        case DeviceHub::State::error: return "error: " + i.error;
        case DeviceHub::State::missing: return "not connected";
        case DeviceHub::State::closed: return "closed";
    }
    return {};
}

juce::Colour stateColour(DeviceHub::State s)
{
    return s == DeviceHub::State::open ? ok : s == DeviceHub::State::opening ? amber : s == DeviceHub::State::closed ? dim : red;
}

// VenueTuner .tag: outlined in its colour; filled with black text when on.
void tag(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& label, bool on, juce::Colour c)
{
    g.setColour(on ? c : dim);
    if (on) g.fillRect(r);
    g.drawRect(r);
    g.setColour(on ? juce::Colours::black : dim);
    g.setFont(mono(9.5f, on));
    g.drawText(label, r, juce::Justification::centred);
}
}

// ---------------------------------------------------------------- one input

class RoutingPage::Row : public juce::Component
{
public:
    Row(RoutingPage& p, const Input& in, bool isFocused) : page(p), input(in), focused(isFocused)
    {
        addAndMakeVisible(iface);
        iface.setTextWhenNothingSelected("choose interface...");
        int item = 1, selected = 0;
        bool listed = false;
        for (auto& i : page.interfaces) {
            const bool plain = i.state == DeviceHub::State::open || i.state == DeviceHub::State::closed;
            iface.addItem(i.name + (plain ? juce::String() : "  (" + stateText(i) + ")"), item);
            if (i.name == input.port.device) { selected = item; listed = true; }
            ++item;
        }
        if (input.port.valid() && !listed) { // patched to an interface that is not here
            iface.addItem(input.port.device + "  (not connected)", item);
            selected = item;
        }
        iface.setSelectedId(selected, juce::dontSendNotification);
        iface.onChange = [this] {
            const int idx = iface.getSelectedId() - 1;
            if (idx < 0 || idx >= int(page.interfaces.size())) return;
            const auto device = page.interfaces[size_t(idx)].name;
            const int channels = page.interfaces[size_t(idx)].inputs.size();
            page.edit([id = input.id, device, channels](Settings& s) {
                const int ch = s.inputs.nextFreeChannel(device, channels);
                if (auto* i = s.inputs.find(id)) i->port = { device, ch };
                s.inputs.focusedId = id;
                s.lastDevice = device;
            });
        };
    }

    void resized() override { iface.setBounds(xIface, 6, wIface, kRowH - 12); }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour(focused ? well : panel);
        g.fillRect(r);
        if (focused) {
            g.setColour(input.colour);
            g.fillRect(r.removeFromLeft(2));
        }
        g.setColour(line);
        g.drawHorizontalLine(getHeight() - 1, 0, float(getWidth()));

        swatch(g, { xSwatch, kRowH / 2 - 6, 12, 12 }, input.muted ? dim : input.colour);
        g.setColour(input.muted ? dim : text);
        g.setFont(mono(11, true));
        g.drawText(input.name.toUpperCase(), xName, 0, wName, kRowH, juce::Justification::centredLeft, true);

        g.setFont(mono(11));
        if (input.port.valid()) {
            const auto* info = page.interfaceInfo(input.port.device);
            const auto chName = info && input.port.channel < info->inputs.size() ? info->inputs[input.port.channel] : juce::String();
            g.setColour(text);
            g.drawText("CH " + juce::String(input.port.channel + 1) + "  " + chName, xChan, 0, wChan, kRowH, juce::Justification::centredLeft, true);
            meter(g, { xMeter, kRowH / 2 - 4, wMeter, 8 }, page.levelDb(input.port));
        } else {
            g.setColour(amber);
            g.drawText("NOT PATCHED", xChan, 0, wChan, kRowH, juce::Justification::centredLeft);
        }
        tag(g, splRect(), "SPL", input.splMic, amber);
        tag(g, showRect(), input.muted ? "HIDDEN" : "SHOWN", !input.muted, accent);
        tag(g, removeRect(), "X", false, red);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        const int id = input.id;
        if (juce::Rectangle<int>(xSwatch - 2, 0, 18, kRowH).contains(p)) { colourMenu(); return; }
        if (juce::Rectangle<int>(xName, 0, wName, kRowH).contains(p) && focused) { rename(); return; }
        if (splRect().contains(p)) { page.edit([id](Settings& s) { const auto* i = s.inputs.find(id); s.inputs.setSplMic(i && i->splMic ? -1 : id); }); return; }
        if (showRect().contains(p)) { page.edit([id](Settings& s) { if (auto* i = s.inputs.find(id)) i->muted = !i->muted; }); return; }
        if (removeRect().contains(p)) { page.edit([id](Settings& s) { s.inputs.remove(id); }); return; }
        page.edit([id](Settings& s) { s.inputs.focusedId = id; });
    }

private:
    static juce::Rectangle<int> splRect() { return { xTags, kRowH / 2 - 8, 36, 16 }; }
    static juce::Rectangle<int> showRect() { return { xTags + 42, kRowH / 2 - 8, 58, 16 }; }
    static juce::Rectangle<int> removeRect() { return { xTags + 106, kRowH / 2 - 8, 18, 16 }; }

    void colourMenu()
    {
        juce::PopupMenu m;
        for (int c = 0; c < Palette::kDistinctHues; ++c) m.addColouredItem(c + 1, "Colour " + juce::String(c + 1), Palette::colour(c));
        juce::Component::SafePointer<RoutingPage> safe(&page);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [safe, id = input.id](int r) {
            if (r > 0 && safe) safe->edit([id, r](Settings& s) { if (auto* i = s.inputs.find(id)) i->colour = Palette::colour(r - 1); });
        });
    }

    void rename()
    {
        auto* w = new juce::AlertWindow("Rename input", "", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor("name", input.name);
        w->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<RoutingPage> safe(&page);
        w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w, id = input.id](int r) {
            const auto name = w->getTextEditorContents("name").trim();
            if (r == 1 && safe && name.isNotEmpty())
                safe->edit([id, name](Settings& s) { if (auto* i = s.inputs.find(id)) i->name = name.substring(0, 32); });
        }), true);
    }

    RoutingPage& page;
    Input input;
    bool focused;
    juce::ComboBox iface;
};

// ---------------------------------------------------------------- channels of the focused row's interface

class RoutingPage::Strip : public juce::Component
{
public:
    static constexpr int kCellW = 168, kCellH = 34, kHead = 26;

    Strip(RoutingPage& p, const Input* in) : page(p)
    {
        if (in) { input = *in; has = true; }
    }

    int heightFor(int width) const
    {
        const auto* info = has && input.port.valid() ? page.interfaceInfo(input.port.device) : nullptr;
        if (!info || info->inputs.isEmpty()) return kHead + 30;
        const int cols = std::max(1, (width - 2 * kPad) / kCellW);
        return kHead + ((info->inputs.size() + cols - 1) / cols) * kCellH + 10;
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(bg);
        auto head = getLocalBounds().removeFromTop(kHead).reduced(kPad, 0);
        if (!has) return;
        if (!input.port.valid()) {
            caption(g, input.name + ": choose an interface above", head);
            return;
        }
        const auto* info = page.interfaceInfo(input.port.device);
        caption(g, input.port.device + "  -  click a channel for " + input.name, head);
        if (!info || info->inputs.isEmpty()) {
            g.setColour(red);
            g.setFont(mono(11));
            g.drawText("interface not connected", kPad, kHead, getWidth() - 2 * kPad, 24, juce::Justification::centredLeft);
            return;
        }
        if (info->state != DeviceHub::State::open) {
            g.setColour(stateColour(info->state));
            g.setFont(mono(10));
            g.drawText(stateText(*info).toUpperCase(), head, juce::Justification::centredRight);
        }
        for (int ch = 0; ch < info->inputs.size(); ++ch) {
            const auto r = cell(ch);
            const bool on = input.port.channel == ch;
            g.setColour(on ? well : panel);
            g.fillRect(r);
            g.setColour(on ? input.colour : line);
            g.drawRect(r, on ? 2 : 1);
            g.setColour(on ? text : dim);
            g.setFont(mono(10.5f, on));
            g.drawText(juce::String(ch + 1).paddedLeft(' ', 2) + " " + info->inputs[ch], r.reduced(6, 0).withTrimmedBottom(12),
                       juce::Justification::centredLeft, true);
            meter(g, r.reduced(6, 0).removeFromBottom(12).withTrimmedBottom(5), page.levelDb({ input.port.device, ch }));
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!has || !input.port.valid()) return;
        const auto* info = page.interfaceInfo(input.port.device);
        if (!info) return;
        for (int ch = 0; ch < info->inputs.size(); ++ch)
            if (cell(ch).contains(e.getPosition())) {
                page.edit([id = input.id, device = input.port.device, ch](Settings& s) {
                    if (auto* i = s.inputs.find(id)) i->port = { device, ch };
                });
                return;
            }
    }

    juce::String device() const { return has && input.port.valid() ? input.port.device : juce::String(); }

private:
    juce::Rectangle<int> cell(int ch) const
    {
        const int cols = std::max(1, (getWidth() - 2 * kPad) / kCellW);
        return { kPad + (ch % cols) * kCellW, kHead + (ch / cols) * kCellH, kCellW - 4, kCellH - 4 };
    }

    RoutingPage& page;
    Input input;
    bool has = false;
};

// ---------------------------------------------------------------- scrolling content

class RoutingPage::List : public juce::Component
{
public:
    static constexpr int kHeader = 22;

    explicit List(RoutingPage& p) : page(p) {}

    void rebuild(int width)
    {
        rows.clear();
        const auto& l = page.hooks.get().inputs;
        for (const auto& in : l.inputs) {
            rows.push_back(std::make_unique<Row>(page, in, in.id == l.focusedId));
            addAndMakeVisible(*rows.back());
        }
        strip = std::make_unique<Strip>(page, l.find(l.focusedId));
        addAndMakeVisible(*strip);
        layout(width);
    }

    void layout(int width)
    {
        int y = kHeader;
        for (auto& r : rows) {
            r->setBounds(0, y, width, kRowH);
            y += kRowH;
        }
        y += 10;
        const int h = rows.empty() || !strip ? 0 : strip->heightFor(width);
        if (strip) strip->setBounds(0, y, width, h);
        setSize(width, std::max(y + h + 8, 120));
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(bg);
        g.setColour(statusBg);
        g.fillRect(0, 0, getWidth(), kHeader);
        g.setColour(line);
        g.drawHorizontalLine(kHeader - 1, 0, float(getWidth()));
        caption(g, "Input", { xName, 0, wName, kHeader });
        caption(g, "Interface", { xIface, 0, wIface, kHeader });
        caption(g, "Channel", { xChan, 0, wChan, kHeader });
        caption(g, "Level", { xMeter, 0, wMeter, kHeader });
        if (rows.empty()) {
            g.setColour(dim);
            g.setFont(mono(12));
            g.drawText("NO INPUTS  -  + ADD INPUT, THEN PICK AN INTERFACE AND A CHANNEL", 0, kHeader + 20, getWidth(), 24,
                       juce::Justification::centred);
        }
    }

    void repaintMeters()
    {
        for (auto& r : rows) r->repaint(xMeter - 2, 0, wMeter + 4, kRowH);
        if (strip) strip->repaint();
    }

    juce::String stripDevice() const { return strip ? strip->device() : juce::String(); }

private:
    RoutingPage& page;
    std::vector<std::unique_ptr<Row>> rows;
    std::unique_ptr<Strip> strip;
};

// ---------------------------------------------------------------- page

RoutingPage::RoutingPage(DeviceHub& h, Analyser& a, Hooks hk) : hub(h), analyser(a), hooks(std::move(hk))
{
    list = std::make_unique<List>(*this);
    viewport.setViewedComponent(list.get(), false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    addAndMakeVisible(add);
    add.onClick = [this] { addInput(); };
    for (auto* box : { &deltaA, &deltaB }) addAndMakeVisible(*box);
    deltaA.onChange = [this] { const int v = deltaA.getSelectedId(); edit([v](Settings& s) { s.inputs.deltaA = v > 1 ? v - 1 : -1; }); };
    deltaB.onChange = [this] { const int v = deltaB.getSelectedId(); edit([v](Settings& s) { s.inputs.deltaB = v > 1 ? v - 1 : -1; }); };
    refresh();
    startTimerHz(20);
}

RoutingPage::~RoutingPage()
{
    stopTimer();
    if (shown && hooks.setVisibleForMeters) hooks.setVisibleForMeters(false);
    viewport.setViewedComponent(nullptr, false);
}

const DeviceHub::Info* RoutingPage::interfaceInfo(const juce::String& name) const
{
    for (auto& i : interfaces)
        if (i.name == name) return &i;
    return nullptr;
}

float RoutingPage::levelDb(const PortRef& p) const
{
    auto it = levels.find(p);
    return it == levels.end() ? kFloorDb : it->second;
}

void RoutingPage::refresh()
{
    interfaces = hub.snapshot();
    list->rebuild(std::max(600, viewport.getMaximumVisibleWidth()));
    add.setEnabled(int(hooks.get().inputs.inputs.size()) < InputList::kMax);
    rebuildDeltaBoxes();
    repaint();
}

// Edits are applied after the current event: the change rebuilds the rows, and the row (or
// combo) that triggered it must not be deleted while its own handler is still running.
void RoutingPage::edit(const std::function<void(Settings&)>& f)
{
    juce::Component::SafePointer<RoutingPage> safe(this);
    juce::MessageManager::callAsync([safe, f] {
        if (!safe) return;
        auto s = safe->hooks.get();
        f(s);
        safe->hooks.set(s); // comes back through refresh()
    });
}

void RoutingPage::addInput()
{
    edit([this](Settings& s) {
        juce::String device = interfaceInfo(s.lastDevice) ? s.lastDevice : juce::String();
        if (device.isEmpty() && !interfaces.empty()) device = interfaces.front().name;
        const auto* info = interfaceInfo(device);
        const int ch = info ? s.inputs.nextFreeChannel(device, info->inputs.size()) : 0;
        if (const int id = s.inputs.add(device.isEmpty() ? PortRef {} : PortRef { device, ch }); id > 0) {
            s.inputs.focusedId = id;
            s.lastDevice = device;
        }
    });
}

// Combo ids: 1 = off, input id + 1 otherwise (ComboBox ids must be non-zero).
void RoutingPage::rebuildDeltaBoxes()
{
    const auto& l = hooks.get().inputs;
    for (auto* box : { &deltaA, &deltaB }) {
        box->clear(juce::dontSendNotification);
        box->addItem("Off", 1);
        for (const auto& in : l.inputs) box->addItem(in.name, in.id + 1);
    }
    deltaA.setSelectedId(l.deltaA > 0 ? l.deltaA + 1 : 1, juce::dontSendNotification);
    deltaB.setSelectedId(l.deltaB > 0 ? l.deltaB + 1 : 1, juce::dontSendNotification);
}

void RoutingPage::timerCallback()
{
    const bool nowShown = isShowing();
    if (nowShown != shown) {
        shown = nowShown;
        if (hooks.setVisibleForMeters) hooks.setVisibleForMeters(shown);
    }
    if (!shown) return;

    // Meters: every channel of every interface on screen (rows + strip), peak-held, −20 dB/s.
    std::set<juce::String> devices;
    for (const auto& in : hooks.get().inputs.inputs)
        if (in.port.valid()) devices.insert(in.port.device);
    if (const auto d = list->stripDevice(); d.isNotEmpty()) devices.insert(d);
    for (const auto& device : devices) {
        auto s = analyser.findSink(device);
        if (!s) continue;
        for (int ch = 0; ch < s->numChannels(); ++ch) {
            const float peak = s->takePeak(ch);
            const float db = peak > 0 ? std::max(kFloorDb, 20.0f * std::log10(peak)) : kFloorDb;
            auto& level = levels.try_emplace(PortRef { device, ch }, kFloorDb).first->second;
            level = std::max(db, level - 1.0f);
        }
    }
    list->repaintMeters();
}

void RoutingPage::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    auto top = getLocalBounds().removeFromTop(40).reduced(kPad, 0);
    caption(g, "Inputs  (" + juce::String(int(hooks.get().inputs.inputs.size())) + " / " + juce::String(InputList::kMax) + ")", top);

    auto r = getLocalBounds().removeFromBottom(64).reduced(kPad, 8);
    g.setColour(line);
    g.drawHorizontalLine(getHeight() - 64, 0, float(getWidth()));
    caption(g, utf8("Delta trace  (A − B, B delayed to line up with A)"), r.removeFromTop(18));
    g.setFont(mono(11));
    g.setColour(dim);
    auto row = r.removeFromTop(28);
    g.drawText("A  FOH MIC", row.removeFromLeft(90), juce::Justification::centredLeft);
    row.removeFromLeft(194);
    g.drawText("B  CONSOLE", row.removeFromLeft(90), juce::Justification::centredLeft);
}

void RoutingPage::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop(40).reduced(kPad, 7);
    add.setBounds(top.removeFromRight(130));
    auto bottom = r.removeFromBottom(64).reduced(kPad, 8);
    viewport.setBounds(r);
    list->layout(std::max(600, viewport.getMaximumVisibleWidth()));
    bottom.removeFromTop(18);
    auto row = bottom.removeFromTop(28);
    row.removeFromLeft(90);
    deltaA.setBounds(row.removeFromLeft(170).reduced(0, 2));
    row.removeFromLeft(24 + 90);
    deltaB.setBounds(row.removeFromLeft(170).reduced(0, 2));
}

} // namespace fs
