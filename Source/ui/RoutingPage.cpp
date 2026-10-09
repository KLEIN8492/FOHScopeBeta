#include "ui/RoutingPage.h"

namespace fs {

using namespace theme;

namespace {
constexpr int kLeft = 236, kCol = 116, kHeader = 84, kRow = 26, kMeterX = 150, kMeterW = 76;
constexpr float kMeterFloorDb = -60.0f;
}

// ---------------------------------------------------------------- matrix

class RoutingPage::Matrix : public juce::Component
{
public:
    explicit Matrix(RoutingPage& p) : page(p) { setRepaintsOnMouseActivity(true); }

    void update(const InputList& l, const juce::StringArray& names)
    {
        inputs = l;
        channels = names;
        levels.resize(size_t(channels.size()), kMeterFloorDb);
        const int cols = int(inputs.inputs.size()) + 1;
        setSize(kLeft + cols * kCol + 12, kHeader + std::max(1, channels.size()) * kRow + 10);
        repaint();
    }

    // Peak-hold meters falling at ~20 dB/s, fed ~20 times a second.
    void meter(int ch, float peak)
    {
        if (ch >= int(levels.size())) return;
        const float db = peak > 0 ? juce::jmax(kMeterFloorDb, 20.0f * std::log10(peak)) : kMeterFloorDb;
        auto& l = levels[size_t(ch)];
        l = std::max(db, l - 1.0f);
    }
    void repaintMeters() { repaint(kMeterX - 2, kHeader, kMeterW + 4, getHeight() - kHeader); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(panel);
        const int n = int(inputs.inputs.size());

        // Column headers.
        for (int j = 0; j < n; ++j) {
            const auto& in = inputs.inputs[size_t(j)];
            const auto card = headerCard(j);
            g.setColour(panelHi);
            g.fillRoundedRectangle(card.toFloat(), 5);
            g.setColour(in.muted ? dim : in.colour);
            g.fillEllipse(dotRect(j).toFloat().reduced(2));
            g.setColour(in.muted ? dim : text);
            g.setFont(sans(13, true));
            g.drawText(in.name, nameRect(j), juce::Justification::centredLeft, true);
            if (in.channel >= channels.size()) {
                g.setColour(red);
                g.setFont(sans(10.5f));
                g.drawText("no ch " + juce::String(in.channel + 1), card.withTrimmedTop(30).withHeight(14).reduced(8, 0),
                           juce::Justification::centredLeft);
            }
            pill(g, splRect(j), "SPL", in.splMic, amber);
            pill(g, showRect(j), in.muted ? "Hidden" : "Shown", !in.muted, in.colour);
            pill(g, removeRect(j), utf8("×"), false, red, n > 1);
        }
        const auto add = addRect();
        const bool canAdd = n < InputList::kMax;
        g.setColour(isMouseOver() && add.contains(getMouseXYRelative()) && canAdd ? panelHi.brighter(0.2f) : panelHi);
        g.fillRoundedRectangle(add.toFloat(), 5);
        g.setColour(canAdd ? text : dim);
        g.setFont(sans(13, true));
        g.drawText("+ Add input", add, juce::Justification::centred);

        g.setColour(dim);
        g.setFont(sans(11, true));
        g.drawText("DEVICE INPUT", 12, kHeader - 20, 120, 16, juce::Justification::centredLeft);
        g.drawText("LEVEL", kMeterX, kHeader - 20, kMeterW, 16, juce::Justification::centredLeft);

        if (channels.isEmpty()) {
            g.setColour(amber);
            g.setFont(sans(13));
            g.drawText(utf8("No audio device — choose one in the Audio tab."), 12, kHeader, kLeft + n * kCol, kRow, juce::Justification::centredLeft);
            return;
        }

        const auto mouse = getMouseXYRelative();
        for (int ch = 0; ch < channels.size(); ++ch) {
            const int y = kHeader + ch * kRow;
            if (ch % 2 == 0) {
                g.setColour(panelHi.withAlpha(0.45f));
                g.fillRect(0, y, getWidth(), kRow);
            }
            g.setColour(text);
            g.setFont(mono(12));
            g.drawText(juce::String(ch + 1).paddedLeft(' ', 2) + "  " + channels[ch], 12, y, kMeterX - 18, kRow,
                       juce::Justification::centredLeft, true);
            paintMeter(g, ch, y);

            for (int j = 0; j < n; ++j) {
                const auto& in = inputs.inputs[size_t(j)];
                const auto cell = cellRect(ch, j);
                const bool on = in.channel == ch, hover = isMouseOver() && cell.contains(mouse);
                g.setColour(on ? in.colour : (hover ? gridStrong.brighter(0.4f) : gridStrong));
                if (on) g.fillRoundedRectangle(cell.toFloat(), 3);
                else g.drawRoundedRectangle(cell.toFloat().reduced(0.5f), 3, hover ? 1.5f : 1.0f);
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        const int n = int(inputs.inputs.size());
        if (addRect().contains(p)) {
            page.edit([this](InputList& l) { if (int id = l.add(channels.size()); id > 0) l.focusedId = id; });
            return;
        }
        for (int j = 0; j < n; ++j) {
            const int id = inputs.inputs[size_t(j)].id;
            if (dotRect(j).contains(p)) { colourMenu(id); return; }
            if (nameRect(j).contains(p)) { rename(id); return; }
            if (splRect(j).contains(p)) { page.edit([id](InputList& l) { const auto* i = l.find(id); l.setSplMic(i && i->splMic ? -1 : id); }); return; }
            if (showRect(j).contains(p)) { page.edit([id](InputList& l) { if (auto* i = l.find(id)) i->muted = !i->muted; }); return; }
            if (removeRect(j).contains(p)) { page.edit([id](InputList& l) { l.remove(id); }); return; }
        }
        for (int ch = 0; ch < channels.size(); ++ch)
            for (int j = 0; j < n; ++j)
                if (cellRect(ch, j).contains(p)) {
                    const int id = inputs.inputs[size_t(j)].id;
                    page.edit([id, ch](InputList& l) { if (auto* i = l.find(id)) i->channel = ch; });
                    return;
                }
    }

private:
    static juce::Rectangle<int> headerCard(int j) { return { kLeft + j * kCol + 4, 6, kCol - 8, kHeader - 30 }; }
    static juce::Rectangle<int> dotRect(int j) { return headerCard(j).withSize(16, 16).translated(8, 8); }
    static juce::Rectangle<int> nameRect(int j) { return headerCard(j).withHeight(28).withTrimmedLeft(28).withTrimmedRight(4).translated(0, 2); }
    static juce::Rectangle<int> pillRow(int j) { return headerCard(j).removeFromBottom(22).reduced(6, 3); }
    static juce::Rectangle<int> splRect(int j) { return pillRow(j).removeFromLeft(34); }
    static juce::Rectangle<int> showRect(int j) { return pillRow(j).withTrimmedLeft(38).withTrimmedRight(24); }
    static juce::Rectangle<int> removeRect(int j) { return pillRow(j).removeFromRight(20); }
    juce::Rectangle<int> addRect() const { return { kLeft + int(inputs.inputs.size()) * kCol + 4, 6, kCol - 8, kHeader - 30 }; }
    static juce::Rectangle<int> cellRect(int ch, int j) { return juce::Rectangle<int>(kLeft + j * kCol, kHeader + ch * kRow, kCol, kRow).withSizeKeepingCentre(16, 16); }

    void pill(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& label, bool on, juce::Colour c, bool enabled = true)
    {
        g.setColour(on ? c.withAlpha(0.85f) : panel);
        g.fillRoundedRectangle(r.toFloat(), 4);
        g.setColour(on ? bg : (enabled ? dim : gridStrong));
        g.setFont(sans(10.5f, true));
        g.drawText(label, r, juce::Justification::centred);
    }

    void paintMeter(juce::Graphics& g, int ch, int y)
    {
        const auto bar = juce::Rectangle<float>(float(kMeterX), float(y + kRow / 2 - 4), float(kMeterW), 8);
        g.setColour(bg);
        g.fillRoundedRectangle(bar, 2);
        const float db = levels[size_t(ch)];
        const float w = bar.getWidth() * (db - kMeterFloorDb) / -kMeterFloorDb;
        g.setColour(db > -3 ? red : db > -12 ? amber : green);
        g.fillRoundedRectangle(bar.withWidth(std::max(0.0f, w)), 2);
    }

    void colourMenu(int id)
    {
        juce::PopupMenu m;
        for (int c = 0; c < Palette::kDistinctHues; ++c) m.addColouredItem(c + 1, "Colour " + juce::String(c + 1), Palette::colour(c));
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [this, id](int r) {
            if (r > 0) page.edit([id, r](InputList& l) { if (auto* i = l.find(id)) i->colour = Palette::colour(r - 1); });
        });
    }

    void rename(int id)
    {
        const auto* in = inputs.find(id);
        if (!in) return;
        auto* w = new juce::AlertWindow("Rename input", "", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor("name", in->name);
        w->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<Matrix> safe(this);
        w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w, id](int r) {
            const auto name = w->getTextEditorContents("name").trim();
            if (r == 1 && safe && name.isNotEmpty())
                safe->page.edit([id, name](InputList& l) { if (auto* i = l.find(id)) i->name = name.substring(0, 32); });
        }), true);
    }

    RoutingPage& page;
    InputList inputs;
    juce::StringArray channels;
    std::vector<float> levels;
};

// ---------------------------------------------------------------- page

RoutingPage::RoutingPage(Hooks h) : hooks(std::move(h))
{
    matrix = std::make_unique<Matrix>(*this);
    viewport.setViewedComponent(matrix.get(), false);
    viewport.setScrollBarsShown(true, true);
    addAndMakeVisible(viewport);
    for (auto* box : { &deltaA, &deltaB }) addAndMakeVisible(*box);
    deltaA.onChange = [this] { edit([this](InputList& l) { l.deltaA = deltaA.getSelectedId() > 1 ? deltaA.getSelectedId() - 1 : -1; }); };
    deltaB.onChange = [this] { edit([this](InputList& l) { l.deltaB = deltaB.getSelectedId() > 1 ? deltaB.getSelectedId() - 1 : -1; }); };
    refresh();
    startTimerHz(20);
}

RoutingPage::~RoutingPage() { viewport.setViewedComponent(nullptr, false); }

void RoutingPage::refresh()
{
    matrix->update(hooks.get().inputs, hooks.channelNames());
    rebuildDeltaBoxes();
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

void RoutingPage::edit(const std::function<void(InputList&)>& f)
{
    auto s = hooks.get();
    f(s.inputs);
    hooks.set(s); // comes back through refresh()
}

void RoutingPage::timerCallback()
{
    if (!isShowing()) return;
    const int n = hooks.channelNames().size();
    for (int ch = 0; ch < n; ++ch) matrix->meter(ch, hooks.takePeak(ch));
    matrix->repaintMeters();
}

void RoutingPage::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    auto r = getLocalBounds().removeFromBottom(64).reduced(16, 8);
    g.setColour(gridStrong);
    g.drawHorizontalLine(getHeight() - 64, 0, float(getWidth()));
    g.setColour(dim);
    g.setFont(sans(12, true));
    g.drawText(utf8("DELTA TRACE  (A − B, B delayed to line up with A)"), r.removeFromTop(18), juce::Justification::centredLeft);
    g.setFont(sans(13));
    g.setColour(text);
    auto row = r.removeFromTop(28);
    g.drawText("A (FOH mic)", row.removeFromLeft(90), juce::Justification::centredLeft);
    row.removeFromLeft(170);
    row.removeFromLeft(24);
    g.drawText("B (console)", row.removeFromLeft(90), juce::Justification::centredLeft);
}

void RoutingPage::resized()
{
    auto r = getLocalBounds();
    auto bottom = r.removeFromBottom(64).reduced(16, 8);
    viewport.setBounds(r);
    bottom.removeFromTop(18);
    auto row = bottom.removeFromTop(28);
    row.removeFromLeft(90);
    deltaA.setBounds(row.removeFromLeft(170).reduced(0, 2));
    row.removeFromLeft(24 + 90);
    deltaB.setBounds(row.removeFromLeft(170).reduced(0, 2));
}

} // namespace fs
