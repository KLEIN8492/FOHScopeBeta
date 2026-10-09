#pragma once
#include "core/Analyser.h"
#include "core/DeviceHub.h"
#include "core/LevelLogger.h"
#include "core/Settings.h"
#include "ui/InputBar.h"
#include "ui/SettingsWindow.h"
#include "ui/SidePanel.h"
#include "ui/SpectrographView.h"
#include "ui/SpectrumView.h"

namespace fs {

class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

    void showSettings();
    void toggleFullScreen();

private:
    void timerCallback() override;
    void apply(const Settings&);   // push settings to analyser + views, save
    void devicesChanged();
    void updateWanted();
    void logLevels(const Frame&);
    void capture();
    void updateBar();
    void showFrame(const Frame&, bool withSidePanel);
    void setScale(DbScale);
    void fitScale();
    std::optional<float> loudestDb(const Frame&) const;
    static juce::uint32 now();

    Analyser analyser;
    DeviceHub hub { analyser }; // after the analyser: closes every interface before it goes
    std::vector<DeviceHub::Info> interfaces;
    bool routingVisible = false;
    Settings settings;
    LevelLogger logger { Settings::directory().getChildFile("levels") };
    juce::String logStatus;

    InputBar bar;
    SpectrumView spectrum;
    SpectrographView spectrograph;
    SidePanel side;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<SettingsWindow> settingsWindow;
    int sideDivider = 0, spectrographDivider = 0;

    std::vector<SpectrumView::Capture> captures;
    bool frozen = false;
    bool autoScale = false;      // dB scale follows the material; always off at launch
    juce::uint32 scaleDirtyMs = 0;
    uint64_t lastSeq = 0;
    juce::uint32 lastLogMs = 0;
    bool deviceLost = false;
    std::vector<float> column;
};

} // namespace fs
