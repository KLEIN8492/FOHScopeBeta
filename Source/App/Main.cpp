#include "ui/MainComponent.h"

namespace {

// App-wide commands. Going through the command manager puts the shortcuts on the macOS menu
// items themselves, so ⌘, works whichever window (main, Settings) has focus.
enum CommandIds : juce::CommandID { kSettings = 0x2001, kFullScreen };

class MainWindow : public juce::DocumentWindow, private juce::MenuBarModel
{
public:
    explicit MainWindow(juce::ApplicationCommandManager& cm)
        : juce::DocumentWindow("FOHScope", fs::theme::bg, juce::DocumentWindow::allButtons), commands(cm)
    {
        setUsingNativeTitleBar(true);
        content = new fs::MainComponent();
        setContentOwned(content, true);
        setResizable(true, false);
        setResizeLimits(900, 560, 10000, 10000);
        centreWithSize(getWidth(), getHeight());

        setApplicationCommandManagerToWatch(&commands);
        appleExtras.addCommandItem(&commands, kSettings, fs::theme::utf8("Settings…"));
        juce::MenuBarModel::setMacMainMenu(this, &appleExtras);
        setVisible(true);
        content->grabKeyboardFocus();
    }

    ~MainWindow() override { juce::MenuBarModel::setMacMainMenu(nullptr); }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    fs::MainComponent& main() { return *content; }

private:
    juce::StringArray getMenuBarNames() override { return { "View" }; }
    juce::PopupMenu getMenuForIndex(int, const juce::String&) override
    {
        juce::PopupMenu m;
        m.addCommandItem(&commands, kSettings, fs::theme::utf8("Settings…"));
        m.addCommandItem(&commands, kFullScreen, "Full Screen");
        return m;
    }
    void menuItemSelected(int, int) override {} // all items are commands

    juce::ApplicationCommandManager& commands;
    juce::PopupMenu appleExtras;
    fs::MainComponent* content = nullptr;
};

class FOHScopeApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "FOHScope"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String&) override
    {
        commands.registerAllCommandsForTarget(this);
        window = std::make_unique<MainWindow>(commands);
    }
    void shutdown() override { window = nullptr; }
    void systemRequestedQuit() override { quit(); }

    void getAllCommands(juce::Array<juce::CommandID>& ids) override
    {
        juce::JUCEApplication::getAllCommands(ids); // quit
        ids.addArray({ kSettings, kFullScreen });
    }

    void getCommandInfo(juce::CommandID id, juce::ApplicationCommandInfo& info) override
    {
        if (id == kSettings) {
            info.setInfo(fs::theme::utf8("Settings…"), "Open the settings window", "App", 0);
            info.addDefaultKeypress(',', juce::ModifierKeys::commandModifier);
        } else if (id == kFullScreen) {
            info.setInfo("Full Screen", "Toggle full screen", "View", 0);
            info.addDefaultKeypress('f', juce::ModifierKeys::commandModifier | juce::ModifierKeys::ctrlModifier);
        } else {
            juce::JUCEApplication::getCommandInfo(id, info);
        }
    }

    bool perform(const InvocationInfo& info) override
    {
        if (!window) return false;
        if (info.commandID == kSettings) { window->main().showSettings(); return true; }
        if (info.commandID == kFullScreen) { window->main().toggleFullScreen(); return true; }
        return juce::JUCEApplication::perform(info);
    }

private:
    juce::ApplicationCommandManager commands;
    std::unique_ptr<MainWindow> window;
};

} // namespace

START_JUCE_APPLICATION(FOHScopeApp)
