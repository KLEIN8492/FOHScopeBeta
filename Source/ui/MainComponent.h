#pragma once
#include "core/Analyser.h"
#include "core/LevelLogger.h"
#include "core/Settings.h"
#include "ui/InputBar.h"
#include "ui/SettingsWindow.h"
#include "ui/SidePanel.h"
#include "ui/SpectrographView.h"
#include "ui/SpectrumView.h"

namespace fs {

class MainComponent : public juce::Component, private juce::Timer, private juce::ChangeListener
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
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void apply(const Settings&);   // push settings to analyser + views, save
    void openDevice();
    void logLevels(const Frame&);
    void capture();
    void updateBar();
    juce::StringArray channelNames() const;

    juce::AudioDeviceManager devices;
    Analyser analyser;
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
    uint64_t lastSeq = 0;
    juce::uint32 lastLogMs = 0, lastRetryMs = 0;
    bool deviceLost = false;
    std::vector<float> column;
};

} // namespace fs
