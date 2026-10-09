#include "ui/MainComponent.h"

namespace {

enum MenuIds { kSettings = 1, kFullScreen };

class MainWindow : public juce::DocumentWindow, private juce::MenuBarModel
{
public:
    MainWindow() : juce::DocumentWindow("FOHScope", fs::theme::bg, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        content = new fs::MainComponent();
        setContentOwned(content, true);
        setResizable(true, false);
        setResizeLimits(900, 560, 10000, 10000);
        centreWithSize(getWidth(), getHeight());
        juce::PopupMenu appleExtras;
        appleExtras.addItem(kSettings, fs::theme::utf8("Settings…  ⌘,"));
        juce::MenuBarModel::setMacMainMenu(this, &appleExtras);
        setVisible(true);
        content->grabKeyboardFocus();
    }

    ~MainWindow() override { juce::MenuBarModel::setMacMainMenu(nullptr); }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

private:
    juce::StringArray getMenuBarNames() override { return { "View" }; }
    juce::PopupMenu getMenuForIndex(int, const juce::String&) override
    {
        juce::PopupMenu m;
        m.addItem(kSettings, fs::theme::utf8("Settings…"));
        m.addItem(kFullScreen, "Full screen  (F)");
        return m;
    }
    void menuItemSelected(int id, int) override
    {
        if (id == kSettings) content->showSettings();
        if (id == kFullScreen) content->toggleFullScreen();
    }

    fs::MainComponent* content = nullptr;
};

class FOHScopeApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "FOHScope"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String&) override { window = std::make_unique<MainWindow>(); }
    void shutdown() override { window = nullptr; }
    void systemRequestedQuit() override { quit(); }

private:
    std::unique_ptr<MainWindow> window;
};

} // namespace

START_JUCE_APPLICATION(FOHScopeApp)
