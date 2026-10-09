#include "ui/SettingsWindow.h"

namespace fs {

using namespace theme;

class SettingsWindow::Measurement : public juce::Component
{
public:
    explicit Measurement(Hooks h) : hooks(std::move(h))
    {
        addAndMakeVisible(history);
        history.addItem("10 s", 10);
        history.addItem("30 s", 30);
        history.addItem("60 s", 60);
        history.onChange = [this] { edit([this](Settings& s) { s.spectrogramSeconds = history.getSelectedId(); }); };

        slider(offset, 60, 180, 0.1, " dB", [](Settings& s, double v) { s.splOffset = v; s.calibration = "offset"; });
        slider(laeq, 70, 120, 0.5, " dB(A)", [](Settings& s, double v) { s.limitLAeq15 = v; });
        slider(lceq, 80, 135, 0.5, " dB(C)", [](Settings& s, double v) { s.limitLCeq15 = v; });
        slider(threshold, 4, 20, 0.5, " dB", [](Settings& s, double v) { s.peaks.thresholdDb = float(v); });
        slider(hold, 0.3, 5, 0.1, " s", [](Settings& s, double v) { s.peaks.holdSec = float(v); });
        slider(floorDb, -150, -40, 1, " dBFS", [](Settings& s, double v) { s.spectrogramFloor = float(v); });
        slider(topDb, -100, 0, 1, " dBFS", [](Settings& s, double v) { s.spectrogramTop = float(v); });
        slider(displayTop, -40, 20, 1, " dBFS", [](Settings& s, double v) { s.displayTop = float(v); });
        slider(displayRange, 30, 150, 5, " dB", [](Settings& s, double v) { s.displayRange = float(v); });

        for (auto* b : { &cal94, &cal114, &refSet, &openLogs }) addAndMakeVisible(*b);
        cal94.onClick = [this] { calibrate(94.0, 0, "calibrator"); };
        cal114.onClick = [this] { calibrate(114.0, 0, "calibrator"); };
        // Reference: a trusted meter (or a known ambient) says the room is X dB(A) right now.
        refLevel.setSliderStyle(juce::Slider::LinearHorizontal);
        refLevel.setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, 22);
        refLevel.setRange(20, 140, 0.1);
        refLevel.setValue(94, juce::dontSendNotification);
        refLevel.setTextValueSuffix(" dB(A)");
        addAndMakeVisible(refLevel);
        refSet.onClick = [this] { calibrate(refLevel.getValue(), 1, "reference"); };
        openLogs.onClick = [] {
            const auto dir = Settings::directory().getChildFile("levels");
            dir.createDirectory();
            dir.startAsProcess();
        };
        addAndMakeVisible(calStatus);
        calStatus.setColour(juce::Label::textColourId, dim);
        refresh();
    }

    void refresh()
    {
        const auto& s = hooks.get();
        history.setSelectedId(s.spectrogramSeconds, juce::dontSendNotification);
        offset.setValue(s.splOffset, juce::dontSendNotification);
        laeq.setValue(s.limitLAeq15, juce::dontSendNotification);
        lceq.setValue(s.limitLCeq15, juce::dontSendNotification);
        threshold.setValue(s.peaks.thresholdDb, juce::dontSendNotification);
        hold.setValue(s.peaks.holdSec, juce::dontSendNotification);
        floorDb.setValue(s.spectrogramFloor, juce::dontSendNotification);
        topDb.setValue(s.spectrogramTop, juce::dontSendNotification);
        displayTop.setValue(s.displayTop, juce::dontSendNotification);
        displayRange.setValue(s.displayRange, juce::dontSendNotification);
        const auto* mic = s.inputs.splMic();
        calStatus.setText(mic ? "SPL mic: " + mic->name + utf8("  ·  ") + s.calibration : "No SPL mic: right-click an input > Use as SPL mic",
                          juce::dontSendNotification);
        cal94.setEnabled(mic != nullptr);
        cal114.setEnabled(mic != nullptr);
        refSet.setEnabled(mic != nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(panel);
        g.setFont(sans(12, true));
        for (auto& [y, title] : headings) {
            g.setColour(dim);
            g.drawText(title, 16, y, getWidth() - 32, 20, juce::Justification::centredLeft);
            g.setColour(gridStrong);
            g.drawHorizontalLine(y + 21, 16, float(getWidth() - 16));
        }
        g.setFont(sans(13));
        g.setColour(text);
        for (auto& [y, label] : labels) g.drawText(label, 16, y, 170, 26, juce::Justification::centredLeft);
    }

    void resized() override
    {
        headings.clear();
        labels.clear();
        int y = 10;
        auto heading = [&](const juce::String& t) { y += 8; headings.push_back({ y, t }); y += 28; };
        auto row = [&](const juce::String& label, juce::Component& c, int w = 0) {
            labels.push_back({ y, label });
            c.setBounds(190, y, w > 0 ? w : getWidth() - 206, 26);
            y += 32;
        };
        heading("SPL CALIBRATION");
        calStatus.setBounds(16, y, getWidth() - 32, 22);
        y += 26;
        labels.push_back({ y, "Calibrator on the mic" });
        cal94.setBounds(190, y, 110, 26);
        cal114.setBounds(306, y, 110, 26);
        y += 32;
        labels.push_back({ y, "Room reads now (LA Slow)" });
        refLevel.setBounds(190, y, getWidth() - 206 - 92, 26);
        refSet.setBounds(getWidth() - 16 - 86, y, 86, 26);
        y += 32;
        row("Offset (dB SPL at 0 dB RMS)", offset);
        heading("NOISE LIMITS (sliding 15 min)");
        row("LAeq 15 min", laeq);
        row("LCeq 15 min", lceq);
        labels.push_back({ y, "Level logs" });
        openLogs.setBounds(190, y, 160, 26);
        y += 32;
        heading("FEEDBACK FINDER");
        row("Threshold above local", threshold);
        row("Must ring for", hold);
        heading("SPECTROGRAPH");
        row("History", history, 120);
        row("Colour floor", floorDb);
        row("Colour top", topDb);
        heading("SPECTRUM DISPLAY");
        row("Top of scale", displayTop);
        row("Range", displayRange);
        setSize(getWidth(), y + 16);
    }

private:
    void slider(juce::Slider& s, double lo, double hi, double step, const juce::String& suffix, std::function<void(Settings&, double)> apply)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, 22);
        s.setRange(lo, hi, step);
        s.setTextValueSuffix(suffix);
        s.onValueChange = [this, &s, apply] { const double v = s.getValue(); edit([&](Settings& st) { apply(st, v); }); };
        addAndMakeVisible(s);
    }

    void edit(const std::function<void(Settings&)>& f)
    {
        auto s = hooks.get();
        f(s);
        hooks.set(s);
    }

    // offset = known level − what the mic reads now (same weighting, Slow).
    void calibrate(double level, int weighting, const juce::String& method)
    {
        const auto z = hooks.splMicSlow(weighting);
        if (!z || *z < -130) {
            calStatus.setText("No signal on the SPL mic: is the calibrator on and the input live?", juce::dontSendNotification);
            return;
        }
        edit([&](Settings& s) { s.splOffset = level - *z; s.calibration = method; });
        refresh();
    }

    Hooks hooks;
    juce::ComboBox history;
    juce::Slider offset, laeq, lceq, threshold, hold, floorDb, topDb, displayTop, displayRange;
    juce::TextButton cal94 { "94 dB" }, cal114 { "114 dB" }, refSet { "Set" }, openLogs { "Open log folder" };
    juce::Slider refLevel;
    juce::Label calStatus;
    std::vector<std::pair<int, juce::String>> headings, labels;
};

SettingsWindow::SettingsWindow(juce::AudioDeviceManager& dm, Hooks h) : juce::DocumentWindow("FOHScope Settings", panel, closeButton)
{
    auto* tabs = new juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop);
    tabs->setTabBarDepth(32);
    routing = std::make_unique<RoutingPage>(RoutingPage::Hooks { h.get, h.set, h.channelNames, h.takePeak });
    tabs->addTab(utf8("Inputs & Routing"), panel, routing.get(), false);

    auto* audio = new juce::AudioDeviceSelectorComponent(dm, 1, 256, 0, 0, false, false, false, false);
    audio->setSize(560, 520);
    tabs->addTab("Audio", panel, audio, true);

    measurement = std::make_unique<Measurement>(std::move(h));
    measurement->setSize(560, 100);
    auto* viewport = new juce::Viewport();
    viewport->setViewedComponent(measurement.get(), false);
    viewport->setScrollBarsShown(true, false);
    tabs->addTab("Measurement", panel, viewport, true);

    setContentOwned(tabs, false);
    setResizable(true, false);
    setUsingNativeTitleBar(true);
    centreWithSize(820, 680);
    measurement->setSize(580, 100);
}

SettingsWindow::~SettingsWindow() { clearContentComponent(); }

void SettingsWindow::refresh()
{
    routing->refresh();
    measurement->refresh();
}

} // namespace fs
