#include "ui/MainComponent.h"

namespace fs {

using namespace theme;

namespace {
constexpr int kStatusHeight = 20, kBarHeight = 32, kBannerHeight = 24, kSideWidth = 250, kMaxCaptures = 6;
}

MainComponent::MainComponent()
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
    settings = Settings::load();
    column.resize(size_t(grid::kPoints));

    addAndMakeVisible(bar);
    addAndMakeVisible(spectrum);
    addAndMakeVisible(spectrograph);
    addAndMakeVisible(side);
    spectrum.setCaptures(&captures);

    bar.onInputs = [this](const InputList& l) { auto s = settings; s.inputs = l; apply(s); };
    bar.onAveraging = [this](Averaging a) { auto s = settings; s.averaging = a; apply(s); };
    bar.onResolution = [this](int r) { auto s = settings; s.bandsPerOctave = r; apply(s); };
    bar.onBars = [this] { auto s = settings; s.showBars = !s.showBars; apply(s); };
    bar.onDelta = [this] { auto s = settings; s.showDelta = !s.showDelta; apply(s); };
    bar.onFreeze = [this] { frozen = !frozen; updateBar(); repaint(0, 0, getWidth(), kStatusHeight); };
    bar.onCapture = [this] { capture(); };
    bar.onClearCaptures = [this] { captures.clear(); updateBar(); spectrum.repaint(); };
    bar.onSettings = [this] { showSettings(); };
    bar.onAutoScale = [this] {
        autoScale = !autoScale;
        if (autoScale) fitScale();
        updateBar();
    };
    // A manual gesture takes the scale back from AUTO.
    spectrum.onScale = [this](DbScale s) {
        if (autoScale) { autoScale = false; updateBar(); }
        setScale(s);
    };
    spectrum.onFit = [this] { fitScale(); };
    bar.onAdd = [this] {
        showSettings();
        settingsWindow->addInput();
    };

    hub.onChanged = [this] { devicesChanged(); };
    hub.setFormat(settings.sampleRate, settings.bufferSize);
    analyser.startAnalysis();
    hub.start(); // scans and opens on its own thread: the window never waits for a driver
#if JUCE_MAC
    // macOS asks for microphone access once; until it is answered nothing is opened (inputs
    // would read silence).
    juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio, [safe = juce::Component::SafePointer(this)](bool) {
        if (safe) safe->hub.setOpeningAllowed(true);
    });
#else
    hub.setOpeningAllowed(true); // no OS-level microphone gate on Windows / Linux
#endif
    apply(settings);
    setSize(1400, 860);
    startTimerHz(60);
}

MainComponent::~MainComponent()
{
    stopTimer();
    hub.onChanged = nullptr;
    settingsWindow = nullptr;
    settings.save();
}

// Interfaces appeared / disappeared / changed state (hub, message thread).
void MainComponent::devicesChanged()
{
    interfaces = hub.snapshot();
    updateWanted(); // the catalogue may have changed what "all interfaces" means
    const bool lost = std::any_of(interfaces.begin(), interfaces.end(), [](auto& i) {
        return i.wanted && (i.state == DeviceHub::State::missing || i.state == DeviceHub::State::error) && i.name.isNotEmpty();
    });
    if (lost != deviceLost) {
        deviceLost = lost;
        resized();
    }
    updateBar();
    if (settingsWindow) settingsWindow->refresh();
    repaint(0, 0, getWidth(), kStatusHeight + kBarHeight + kBannerHeight);
}

void MainComponent::updateWanted()
{
    hub.setWanted(wantedDevices(settings.inputs, routingVisible, hub.availableNames()));
}

void MainComponent::apply(const Settings& s)
{
    settings = s;
    analyser.setInputs(settings.inputs);
    analyser.setAveraging(settings.averaging);
    analyser.setBandsPerOctave(settings.bandsPerOctave);
    analyser.setPeakParams(settings.peaks);
    analyser.setSplOffset(settings.splOffset);
    configureSpectrograph();
    hub.setFormat(settings.sampleRate, settings.bufferSize);
    updateWanted();
    settings.save();
    updateBar();
    showFrame(analyser.latest(), true); // inputs / display changed: redraw even without new audio
    if (settingsWindow) settingsWindow->refresh();
}

// dB SPL when the spectrograph shows the SPL mic (the only input with an acoustic level).
void MainComponent::configureSpectrograph()
{
    const auto* focused = settings.inputs.find(settings.inputs.focusedId);
    const auto* mic = settings.inputs.splMic();
    SpectrographView::Units u;
    u.spl = focused && mic && focused->id == mic->id;
    u.splOffset = settings.splOffset;
    u.estimate = settings.calibration == "estimate";
    u.floor = u.spl ? settings.spectrogramFloorSpl : settings.spectrogramFloor;
    u.top = u.spl ? settings.spectrogramTopSpl : settings.spectrogramTop;
    spectrograph.configure(settings.spectrogramSeconds, u);
}

void MainComponent::updateBar()
{
    InputBar::State st;
    st.inputs = settings.inputs;
    st.interfaces = interfaces;
    st.averaging = settings.averaging;
    st.bandsPerOctave = settings.bandsPerOctave;
    st.bars = settings.showBars;
    st.delta = settings.showDelta;
    st.frozen = frozen;
    st.autoScale = autoScale;
    st.captures = int(captures.size());
    bar.setState(st);
    if (const auto* f = settings.inputs.find(settings.inputs.focusedId)) spectrograph.setLabel(f->name, f->colour);
}

void MainComponent::timerCallback()
{
    const auto& f = analyser.latest();
    const bool fresh = f.seq != lastSeq;
    lastSeq = f.seq;
    if (fresh) logLevels(f); // logging never stops, even when the display is frozen
    if (fresh && autoScale && !frozen)
        if (const auto loud = loudestDb(f)) {
            const auto next = DbScale { settings.displayTop, settings.displayRange }.followed(*loud);
            if (next.top != settings.displayTop) setScale(next);
        }
    if (scaleDirtyMs != 0 && now() - scaleDirtyMs > 1000) { // save once the gesture settles
        scaleDirtyMs = 0;
        settings.save();
        if (settingsWindow) settingsWindow->refresh();
    }

    bool added = false;
    while (analyser.popSpectrogramColumn(column.data()))
        if (!frozen) { spectrograph.addColumn(column.data()); added = true; }
    // 15 Hz is plenty for a scrolling heat map and halves its share of the drawing.
    if (added && ++spectrographDivider >= 2) {
        spectrographDivider = 0;
        spectrograph.repaint();
    }
    if (frozen || !fresh) return;

    // Sound level meters display at 10 Hz (readable digits); the analyser publishes at 30.
    showFrame(f, ++sideDivider >= 3);
    if (sideDivider >= 3) sideDivider = 0;
}

juce::uint32 MainComponent::now() { return juce::Time::getMillisecondCounter(); }

void MainComponent::setScale(DbScale s)
{
    s = s.clamped();
    settings.displayTop = s.top;
    settings.displayRange = s.range;
    spectrum.setScale(s);
    scaleDirtyMs = std::max<juce::uint32>(1, now());
}

// Loudest displayed level (dBFS) of the shown traces, if any has data.
std::optional<float> MainComponent::loudestDb(const Frame& f) const
{
    std::optional<float> loud;
    for (const auto& t : f.traces)
        if (t.hasData)
            for (float v : t.line)
                if (v > -190.0f) loud = std::max(loud.value_or(-200.0f), v + theme::kAes17);
    return loud;
}

void MainComponent::fitScale()
{
    if (const auto loud = loudestDb(analyser.latest()))
        setScale(DbScale { settings.displayTop, settings.displayRange }.fittedTo(*loud));
}

void MainComponent::showFrame(const Frame& f, bool withSidePanel)
{
    SpectrumView::View v;
    v.showBars = settings.showBars;
    v.showDelta = settings.showDelta;
    v.top = settings.displayTop;
    v.range = settings.displayRange;
    v.bandsPerOctave = settings.bandsPerOctave;
    spectrum.setData(f, settings.inputs, v);
    if (withSidePanel) side.setData(f, settings.inputs, { settings.limitLAeq15, settings.limitLCeq15, settings.calibration, logStatus });
}

void MainComponent::logLevels(const Frame& f)
{
    const auto now = juce::Time::getMillisecondCounter();
    if (!f.splValid || now - lastLogMs < 1000) return;
    lastLogMs = now;
    const auto& s = f.spl;
    const double o = f.splOffset;
    LevelLogger::Row row { juce::Time::getCurrentTime(), s.leqWindow[0][1] + o, s.leqWindow[0][2] + o,
                           s.leqWindow[SplMeter::kWindows - 1][1] + o, s.leqWindow[SplMeter::kWindows - 1][2] + o, s.cPeak + o,
                           settings.calibration };
    juce::String err;
    logStatus = logger.append(row, err) ? juce::String() : err;
}

void MainComponent::capture()
{
    const auto& f = analyser.latest();
    const auto* in = settings.inputs.find(settings.inputs.focusedId);
    const auto* t = in ? f.trace(in->id) : nullptr;
    if (!t || !t->hasData) return;
    if (int(captures.size()) >= kMaxCaptures) captures.erase(captures.begin());
    captures.push_back({ t->line, in->colour.interpolatedWith(theme::capture, 0.6f),
                         in->name + " " + juce::Time::getCurrentTime().formatted("%H:%M:%S") });
    updateBar();
    spectrum.repaint();
}

void MainComponent::showSettings()
{
    if (!settingsWindow) {
        SettingsWindow::Hooks h;
        h.get = [this]() -> const Settings& { return settings; };
        h.set = [this](const Settings& s) { apply(s); };
        h.splMicSlow = [this](int weighting) -> std::optional<double> {
            const auto& f = analyser.latest();
            if (!f.splValid) return std::nullopt;
            return f.spl.slow[size_t(juce::jlimit(0, 2, weighting))];
        };
        h.setRoutingVisible = [this](bool visible) {
            routingVisible = visible;
            updateWanted();
        };
        settingsWindow = std::make_unique<SettingsWindow>(hub, analyser, std::move(h));
    }
    settingsWindow->refresh();
    settingsWindow->setVisible(true);
    settingsWindow->toFront(true);
}

void MainComponent::toggleFullScreen()
{
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) w->setFullScreen(!w->isFullScreen());
}

bool MainComponent::keyPressed(const juce::KeyPress& k)
{
    const auto c = juce::CharacterFunctions::toLowerCase(k.getTextCharacter());
    if (k.getModifiers().isCommandDown()) return false; // ⌘ shortcuts are menu commands (Main.cpp)
    auto s = settings;
    if (k == juce::KeyPress::spaceKey) { frozen = !frozen; updateBar(); spectrum.repaint(); repaint(0, 0, getWidth(), kStatusHeight); return true; }
    if (c == 'c') { capture(); return true; }
    if (c == 'x') { captures.clear(); updateBar(); spectrum.repaint(); return true; }
    if (c == 'b') { s.showBars = !s.showBars; apply(s); return true; }
    if (c == 'd') { s.showDelta = !s.showDelta; apply(s); return true; }
    if (c == 'r') { analyser.resetAveraging(); spectrograph.clear(); return true; }
    if (c == 'f') { toggleFullScreen(); return true; }
    if (c == 'z') { fitScale(); return true; }
    if (c == 'a') {
        s.averaging = s.averaging == Averaging::Fast ? Averaging::Slow : s.averaging == Averaging::Slow ? Averaging::Infinite : Averaging::Fast;
        apply(s);
        return true;
    }
    if (c >= '1' && c <= '9') {
        const size_t i = size_t(c - '1');
        if (i < s.inputs.inputs.size()) { s.inputs.focusedId = s.inputs.inputs[i].id; apply(s); }
        return true;
    }
    return false;
}

// Top status line, VenueTuner style: what is running, at what rate, and whether it is healthy.
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(bg);
    auto status = getLocalBounds().removeFromTop(kStatusHeight);
    g.setColour(statusBg);
    g.fillRect(status);
    g.setColour(line);
    g.drawHorizontalLine(kStatusHeight - 1, 0, float(getWidth()));

    auto r = status.reduced(8, 0);
    auto item = [&](const juce::String& s, juce::Colour c) {
        g.setColour(c);
        const int w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(mono(11), s)) + 14;
        g.drawText(s, r.removeFromLeft(w), juce::Justification::centredLeft);
    };
    g.setFont(mono(11, true));
    item("FOHSCOPE", text);
    g.setFont(mono(11));
    item(juce::String(settings.sampleRate / 1000.0, 1) + " kHz / " + juce::String(settings.bufferSize), dim);
    // One item per interface in use, coloured by its state.
    for (const auto& i : interfaces) {
        if (!i.wanted || (routingVisible && !std::any_of(settings.inputs.inputs.begin(), settings.inputs.inputs.end(),
                                                          [&](auto& in) { return in.port.device == i.name; })))
            continue;
        const bool good = i.state == DeviceHub::State::open, busy = i.state == DeviceHub::State::opening;
        item(i.name.toUpperCase() + (good ? "" : busy ? " opening" : i.state == DeviceHub::State::missing ? " missing" : " error"),
             good ? text : busy ? amber : red);
    }
    if (settings.inputs.inputs.empty()) item("NO INPUTS", amber);
    else item(juce::String(int(settings.inputs.inputs.size())) + " traces", dim);
    if (settings.inputs.splMic()) item(logStatus.isEmpty() ? "LOG OK" : "LOG FAIL", logStatus.isEmpty() ? ok : red);
    if (frozen) item("FROZEN", amber);
    g.setColour(dim);
    g.drawText(shortcut(",") + " SETUP", status.reduced(8, 0), juce::Justification::centredRight);

    if (deviceLost) {
        auto banner = getLocalBounds().withTrimmedTop(kStatusHeight + kBarHeight).removeFromTop(kBannerHeight);
        g.setColour(red);
        g.fillRect(banner);
        g.setColour(juce::Colours::white);
        g.setFont(mono(11, true));
        juce::StringArray lost;
        for (auto& i : interfaces)
            if (i.wanted && (i.state == DeviceHub::State::missing || i.state == DeviceHub::State::error)) lost.add(i.name.toUpperCase());
        g.drawText(utf8("INTERFACE LOST: ") + lost.joinIntoString(", ") + utf8(" — REOPENING AUTOMATICALLY"), banner.reduced(10, 0),
                   juce::Justification::centredLeft);
    }
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop(kStatusHeight);
    bar.setBounds(r.removeFromTop(kBarHeight));
    if (deviceLost) r.removeFromTop(kBannerHeight);
    side.setBounds(r.removeFromRight(kSideWidth));
    spectrum.setBounds(r.removeFromTop(juce::roundToInt(float(r.getHeight()) * 0.62f)));
    spectrograph.setBounds(r);
}

} // namespace fs
