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

    devices.addChangeListener(this);
    // macOS asks for microphone access once; until it is granted every input reads silence.
    // Opened after the window is up: a driver that is slow to answer (coreaudiod still cleaning
    // up a crashed client, an interface waking up) must not leave you staring at no window.
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer(this)] {
        if (safe)
            juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio, [safe](bool) {
                if (safe) safe->openDevice();
            });
    });
    analyser.startAnalysis();
    apply(settings);
    setSize(1400, 860);
    startTimerHz(60);
}

MainComponent::~MainComponent()
{
    stopTimer();
    devices.removeChangeListener(this);
    devices.removeAudioCallback(&analyser);
    analyser.stopAnalysis();
    if (auto xml = devices.createStateXml()) settings.deviceState = std::move(xml);
    settings.save();
}

void MainComponent::openDevice()
{
    // Ask for every input the device has (256 = "all"): inputs then pick any channel freely.
    const auto err = devices.initialise(256, 0, settings.deviceState.get(), true);
    devices.removeAudioCallback(&analyser);
    devices.addAudioCallback(&analyser);
    if (err.isNotEmpty()) DBG("Audio: " << err);
    updateBar();
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    const bool lost = devices.getCurrentAudioDevice() == nullptr;
    if (lost != deviceLost) {
        deviceLost = lost;
        resized();
    }
    if (!deviceLost)
        if (auto xml = devices.createStateXml()) {
            settings.deviceState = std::move(xml);
            settings.save();
        }
    updateBar();
    if (settingsWindow) settingsWindow->refresh(); // the routing rows follow the device's channels
    repaint();
}

juce::StringArray MainComponent::channelNames() const
{
    if (auto* d = devices.getCurrentAudioDevice()) return d->getInputChannelNames();
    return {};
}

void MainComponent::apply(const Settings& s)
{
    settings = s;
    analyser.setInputs(settings.inputs);
    analyser.setAveraging(settings.averaging);
    analyser.setBandsPerOctave(settings.bandsPerOctave);
    analyser.setPeakParams(settings.peaks);
    analyser.setSplOffset(settings.splOffset);
    spectrograph.configure(settings.spectrogramSeconds, settings.spectrogramFloor, settings.spectrogramTop);
    settings.save();
    updateBar();
    if (settingsWindow) settingsWindow->refresh();
}

void MainComponent::updateBar()
{
    InputBar::State st;
    st.inputs = settings.inputs;
    st.channelNames = channelNames();
    st.averaging = settings.averaging;
    st.bandsPerOctave = settings.bandsPerOctave;
    st.bars = settings.showBars;
    st.delta = settings.showDelta;
    st.frozen = frozen;
    st.captures = int(captures.size());
    bar.setState(st);
    if (const auto* f = settings.inputs.find(settings.inputs.focusedId)) spectrograph.setLabel(f->name, f->colour);
}

void MainComponent::timerCallback()
{
    // Device gone (unplugged interface): retry every 2 s with the saved setup.
    const auto now = juce::Time::getMillisecondCounter();
    if (deviceLost && now - lastRetryMs > 2000) {
        lastRetryMs = now;
        openDevice();
    }

    const auto& f = analyser.latest();
    const bool fresh = f.seq != lastSeq;
    lastSeq = f.seq;
    if (fresh) logLevels(f); // logging never stops, even when the display is frozen

    bool added = false;
    while (analyser.popSpectrogramColumn(column.data()))
        if (!frozen) { spectrograph.addColumn(column.data()); added = true; }
    // 15 Hz is plenty for a scrolling heat map and halves its share of the drawing.
    if (added && ++spectrographDivider >= 2) {
        spectrographDivider = 0;
        spectrograph.repaint();
    }
    if (frozen || !fresh) return;

    SpectrumView::View v;
    v.showBars = settings.showBars;
    v.showDelta = settings.showDelta;
    v.top = settings.displayTop;
    v.range = settings.displayRange;
    v.bandsPerOctave = settings.bandsPerOctave;
    spectrum.setData(f, settings.inputs, v);
    // Sound level meters display at 10 Hz (readable digits); the analyser publishes at 30.
    if (++sideDivider >= 3) {
        sideDivider = 0;
        side.setData(f, settings.inputs, { settings.limitLAeq15, settings.limitLCeq15, settings.calibration, logStatus });
    }
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
        h.channelNames = [this] { return channelNames(); };
        h.takePeak = [this](int ch) { return analyser.takeChannelPeak(ch); };
        settingsWindow = std::make_unique<SettingsWindow>(devices, std::move(h));
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
    if (auto* d = devices.getCurrentAudioDevice()) {
        item(d->getName(), text);
        item(juce::String(d->getCurrentSampleRate() / 1000.0, 1) + " kHz / " + juce::String(d->getCurrentBufferSizeSamples()), dim);
        item(juce::String(d->getInputChannelNames().size()) + " ch in", dim);
    } else {
        item("NO DEVICE", red);
    }
    item(juce::String(int(settings.inputs.inputs.size())) + " traces", dim);
    if (settings.inputs.splMic()) item(logStatus.isEmpty() ? "LOG OK" : "LOG FAIL", logStatus.isEmpty() ? ok : red);
    if (frozen) item("FROZEN", amber);
    g.setColour(dim);
    g.drawText("CMD+, SETUP", status.reduced(8, 0), juce::Justification::centredRight);

    if (deviceLost) {
        auto banner = getLocalBounds().withTrimmedTop(kStatusHeight + kBarHeight).removeFromTop(kBannerHeight);
        g.setColour(red);
        g.fillRect(banner);
        g.setColour(juce::Colours::white);
        g.setFont(mono(11, true));
        g.drawText(utf8("AUDIO DEVICE LOST — RECONNECTING EVERY 2 S"), banner.reduced(10, 0), juce::Justification::centredLeft);
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
