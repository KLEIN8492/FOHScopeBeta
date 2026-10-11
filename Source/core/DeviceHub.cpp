#include "core/DeviceHub.h"

namespace fs {

namespace {
constexpr int kTickMs = 250, kDeadMs = 2000;
bool reached(juce::uint32 now, juce::uint32 due) { return juce::int32(now - due) >= 0; }
}

std::set<juce::String> wantedDevices(const InputList& l, bool routingVisible, const juce::StringArray& available)
{
    std::set<juce::String> w;
    for (const auto& in : l.inputs)
        if (!in.muted && in.port.valid()) w.insert(in.port.device);
    if (routingVisible)
        for (const auto& n : available) w.insert(n);
    return w;
}

int DeviceHub::retryDelayMs(int failures)
{
    return failures <= 0 ? 0 : std::min(30000, 1000 << std::min(failures - 1, 5));
}

DeviceHub::DeviceHub(Analyser& a) : juce::Thread("FOHScope devices"), analyser(a)
{
    // Each factory returns nullptr when its driver is not compiled in (JUCE_ASIO, JUCE_JACK...).
    for (auto* t : { juce::AudioIODeviceType::createAudioIODeviceType_CoreAudio(),
                     juce::AudioIODeviceType::createAudioIODeviceType_ASIO(),
                     juce::AudioIODeviceType::createAudioIODeviceType_WASAPI(juce::WASAPIDeviceMode::shared),
                     juce::AudioIODeviceType::createAudioIODeviceType_ALSA(),
                     juce::AudioIODeviceType::createAudioIODeviceType_JACK() })
        if (t != nullptr) {
            t->addListener(this);
            typeNames.add(t->getTypeName());
            types.add(t);
        }
}

juce::AudioIODeviceType* DeviceHub::typeFor(const juce::String& key, juce::String& deviceName) const
{
    const auto k = splitInterfaceKey(key, typeNames);
    deviceName = k.name;
    const int i = typeNames.indexOf(k.type);
    return i >= 0 ? types[i] : nullptr;
}

DeviceHub::~DeviceHub()
{
    for (auto* t : types) t->removeListener(this);
    cancelPendingUpdate();
    signalThreadShouldExit();
    notify();
    stopThread(15000); // run() closes every interface on its way out
}

// ---------------------------------------------------------------- message thread

void DeviceHub::setFormat(double sr, int block)
{
    {
        std::lock_guard<std::mutex> g(lock);
        if (sr == rate && block == buffer) return;
        rate = sr;
        buffer = block;
        ++formatGeneration;
    }
    notify();
}

void DeviceHub::setWanted(const std::set<juce::String>& w)
{
    {
        std::lock_guard<std::mutex> g(lock);
        if (w == wanted) return;
        wanted = w;
    }
    notify();
}

void DeviceHub::setOpeningAllowed(bool allowed)
{
    {
        std::lock_guard<std::mutex> g(lock);
        openingAllowed = allowed;
    }
    notify();
}

std::vector<DeviceHub::Info> DeviceHub::snapshot() const
{
    std::lock_guard<std::mutex> g(lock);
    std::vector<Info> out;
    for (const auto& [name, inputs] : available) {
        Info i;
        if (auto it = states.find(name); it != states.end()) i = it->second;
        i.name = name;
        i.inputs = inputs;
        i.wanted = wanted.count(name) > 0;
        if (i.state == State::missing) i.state = State::closed; // it is back
        out.push_back(i);
    }
    for (const auto& name : wanted) {
        if (std::any_of(available.begin(), available.end(), [&](auto& a) { return a.first == name; })) continue;
        Info i;
        if (auto it = states.find(name); it != states.end()) i = it->second;
        i.name = name;
        i.state = State::missing;
        i.wanted = true;
        out.push_back(i);
    }
    return out;
}

std::optional<DeviceHub::Info> DeviceHub::find(const juce::String& name) const
{
    for (auto& i : snapshot())
        if (i.name == name) return i;
    return std::nullopt;
}

juce::StringArray DeviceHub::availableNames() const
{
    std::lock_guard<std::mutex> g(lock);
    juce::StringArray names;
    for (const auto& a : available) names.add(a.first);
    return names;
}

double DeviceHub::sampleRate() const
{
    std::lock_guard<std::mutex> g(lock);
    return rate;
}

int DeviceHub::bufferSize() const
{
    std::lock_guard<std::mutex> g(lock);
    return buffer;
}

void DeviceHub::audioDeviceListChanged()
{
    {
        std::lock_guard<std::mutex> g(lock);
        rescanNeeded = true;
    }
    notify();
}

void DeviceHub::handleAsyncUpdate()
{
    if (onChanged) onChanged();
}

// ---------------------------------------------------------------- hub thread

void DeviceHub::run()
{
    while (!threadShouldExit()) {
        bool scan;
        {
            std::lock_guard<std::mutex> g(lock);
            scan = rescanNeeded;
            rescanNeeded = false;
        }
        if (scan) {
            retries.clear(); // interfaces came or went: try them afresh
            rescan();
        }
        reconcile();
        watchdog();
        wait(kTickMs);
    }
    while (!open.empty()) closeOne(open.begin()->first);
}

void DeviceHub::rescan()
{
    std::vector<std::pair<juce::String, juce::StringArray>> found;
    for (auto* type : types) {
        type->scanForDevices();
        for (const auto& name : type->getDeviceNames(true)) {
            const auto key = makeInterfaceKey(type->getTypeName(), name);
            juce::StringArray inputs;
            if (auto it = open.find(key); it != open.end()) {
                inputs = it->second.device->getInputChannelNames();
            } else if (std::unique_ptr<juce::AudioIODevice> probe { type->createDevice({}, name) }) {
                inputs = probe->getInputChannelNames(); // unopened: only reads the channel names
            }
            if (!inputs.isEmpty()) found.emplace_back(key, inputs);
        }
    }
    {
        std::lock_guard<std::mutex> g(lock);
        available = std::move(found);
    }
    triggerAsyncUpdate();
}

void DeviceHub::reconcile()
{
    std::set<juce::String> want;
    std::vector<std::pair<juce::String, juce::StringArray>> avail;
    double sr;
    int block;
    uint64_t fmt;
    bool allowed;
    {
        std::lock_guard<std::mutex> g(lock);
        want = wanted;
        avail = available;
        sr = rate;
        block = buffer;
        fmt = formatGeneration;
        allowed = openingAllowed;
    }
    auto inputsOf = [&](const juce::String& n) -> const juce::StringArray* {
        for (auto& a : avail)
            if (a.first == n) return &a.second;
        return nullptr;
    };

    // Close what is no longer wanted, gone, or open in an old format.
    std::vector<juce::String> toClose;
    for (auto& [name, o] : open)
        if (want.count(name) == 0 || inputsOf(name) == nullptr || openFormatGeneration != fmt) toClose.push_back(name);
    for (auto& name : toClose) {
        closeOne(name);
        setState(name, inputsOf(name) ? State::closed : State::missing);
    }
    if (openFormatGeneration != fmt) retries.clear();
    openFormatGeneration = fmt;

    if (!allowed) return;
    const auto now = juce::Time::getMillisecondCounter();
    for (const auto& name : want) {
        if (threadShouldExit()) return;
        if (open.count(name)) continue;
        const auto* inputs = inputsOf(name);
        if (inputs == nullptr) {
            setState(name, State::missing, "not connected");
            continue;
        }
        if (auto it = retries.find(name); it != retries.end() && !reached(now, it->second.due)) continue;
        setState(name, State::opening);
        if (!openOne(name, *inputs, sr, block)) scheduleRetry(name);
    }
}

void DeviceHub::watchdog()
{
    std::vector<juce::String> dead;
    for (auto& [name, o] : open) {
        const auto beat = o.sink->heartbeat();
        o.stalledMs = beat == o.lastBeat ? o.stalledMs + kTickMs : 0;
        o.lastBeat = beat;
        if (o.stalledMs >= kDeadMs) dead.push_back(name);
    }
    for (auto& name : dead) {
        closeOne(name);
        {
            std::lock_guard<std::mutex> g(lock);
            ++states[name].restarts;
        }
        setState(name, State::error, "stopped delivering audio, reopening");
        scheduleRetry(name);
    }
}

bool DeviceHub::openOne(const juce::String& name, const juce::StringArray& inputs, double sr, int block)
{
    juce::String deviceName;
    auto* type = typeFor(name, deviceName);
    std::unique_ptr<juce::AudioIODevice> device { type ? type->createDevice({}, deviceName) : nullptr }; // input only
    if (device == nullptr) {
        setState(name, State::error, "cannot create device");
        return false;
    }
    juce::BigInteger ins;
    ins.setRange(0, inputs.size(), true);
    if (const auto err = device->open(ins, {}, sr, block); err.isNotEmpty()) {
        setState(name, State::error, err);
        return false;
    }
    // No resampling: an interface must run at the shared rate.
    if (std::abs(device->getCurrentSampleRate() - sr) > 1.0) {
        const auto actual = device->getCurrentSampleRate();
        device->close();
        setState(name, State::error, "runs at " + juce::String(actual / 1000.0, 1) + " kHz, not " + juce::String(sr / 1000.0, 1));
        return false;
    }
    auto sink = analyser.sink(name);
    device->start(sink.get());
    open[name] = { std::move(device), std::move(sink), 0, 0 };
    retries.erase(name);
    setState(name, State::open);
    return true;
}

void DeviceHub::closeOne(const juce::String& name)
{
    auto it = open.find(name);
    if (it == open.end()) return;
    it->second.device->stop();
    it->second.device->close();
    open.erase(it);
}

void DeviceHub::setState(const juce::String& name, State s, const juce::String& error)
{
    {
        std::lock_guard<std::mutex> g(lock);
        auto& i = states[name];
        if (i.state == s && i.error == error) return;
        i.name = name;
        i.state = s;
        i.error = error;
    }
    triggerAsyncUpdate();
}

void DeviceHub::scheduleRetry(const juce::String& name)
{
    auto& r = retries[name];
    ++r.failures;
    r.due = juce::Time::getMillisecondCounter() + juce::uint32(retryDelayMs(r.failures));
}

} // namespace fs
