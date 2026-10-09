#include "core/Analyser.h"

namespace fs {

namespace {
constexpr size_t kFifoSize = 1 << 17; // ≈ 2.7 s at 48 kHz: the analysis thread may stall that long
constexpr int kMaxBlock = 1 << 14;
constexpr double kPublishHz = 30.0;
}

Analyser::Analyser() : juce::Thread("FOHScope analysis") {}

Analyser::~Analyser() { stopAnalysis(); }

// ---------------------------------------------------------------- controls (message thread)

void Analyser::setInputs(const InputList& l)
{
    std::lock_guard<std::mutex> g(controlLock);
    controls.inputs = l;
    ++controlVersion;
}

void Analyser::setAveraging(Averaging a)
{
    std::lock_guard<std::mutex> g(controlLock);
    controls.averaging = a;
    ++controlVersion;
}

void Analyser::setBandsPerOctave(int b)
{
    std::lock_guard<std::mutex> g(controlLock);
    controls.bandsPerOctave = b;
    ++controlVersion;
}

void Analyser::setPeakParams(PeakFinder::Params p)
{
    std::lock_guard<std::mutex> g(controlLock);
    controls.peak = p;
    ++controlVersion;
}

void Analyser::setSplOffset(double dB)
{
    std::lock_guard<std::mutex> g(controlLock);
    controls.splOffset = dB;
    ++controlVersion;
}

bool Analyser::popSpectrogramColumn(float* out)
{
    if (columns.available() < size_t(grid::kPoints)) return false;
    columns.pop(out, size_t(grid::kPoints));
    return true;
}

// ---------------------------------------------------------------- device

void Analyser::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    std::vector<int> map;
    const auto activeIn = device->getActiveInputChannels();
    for (int ch = 0; ch <= activeIn.getHighestBit(); ++ch)
        if (activeIn[ch]) map.push_back(ch);
    prepareDevice(device->getCurrentSampleRate(), device->getInputChannelNames().size(), std::move(map));
}

void Analyser::audioDeviceStopped() { deviceActive.store(false); }

void Analyser::prepareDevice(double sr, int numChannels, std::vector<int> map)
{
    std::lock_guard<std::mutex> g(deviceLock); // the analysis thread holds it while draining
    deviceActive.store(false);
    sampleRate = sr;
    deviceChannels = numChannels;
    activeToDevice = std::move(map);
    fifos.clear();
    for (int ch = 0; ch < numChannels; ++ch) fifos.push_back(std::make_unique<SpscFifo<float>>(kFifoSize));
    deviceGeneration.fetch_add(1);
    deviceActive.store(true);
}

void Analyser::audioDeviceIOCallbackWithContext(const float* const* in, int numIn, float* const* out, int numOut, int n,
                                                const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c)
        if (out[c] != nullptr) juce::FloatVectorOperations::clear(out[c], n);
    if (!deviceActive.load(std::memory_order_acquire)) return;
    for (int c = 0; c < numIn && c < int(activeToDevice.size()); ++c) {
        const int ch = activeToDevice[size_t(c)];
        if (in[c] != nullptr && ch < int(fifos.size())) fifos[size_t(ch)]->push(in[c], size_t(n)); // full → drop
    }
}

void Analyser::prepareForTest(double sr, int numChannels)
{
    std::vector<int> map(static_cast<size_t>(numChannels));
    for (int i = 0; i < numChannels; ++i) map[size_t(i)] = i;
    prepareDevice(sr, numChannels, std::move(map));
}

void Analyser::pushTestBlock(const float* const* ch, int numChannels, int n)
{
    float* none[1] = { nullptr };
    audioDeviceIOCallbackWithContext(ch, numChannels, none, 0, n, {});
}

// ---------------------------------------------------------------- analysis thread

void Analyser::run()
{
    while (!threadShouldExit()) {
        tick();
        wait(10);
    }
}

void Analyser::applyControls(const Controls& c)
{
    // Keep the DSP of inputs that still exist on the same channel (no averaging reset when you
    // add, rename or recolour another input); create the rest.
    std::vector<std::unique_ptr<InputDsp>> next;
    for (const auto& in : c.inputs.inputs) {
        if (in.muted) continue;
        auto it = std::find_if(dsps.begin(), dsps.end(), [&](auto& d) { return d && d->id == in.id && d->channel == in.channel; });
        if (it != dsps.end()) {
            next.push_back(std::move(*it));
        } else {
            auto d = std::make_unique<InputDsp>();
            d->id = in.id;
            d->channel = in.channel;
            d->spectrum.prepare(sampleRate);
            next.push_back(std::move(d));
        }
        next.back()->peaks.setParams(c.peak);
    }
    dsps = std::move(next);

    const auto* mic = c.inputs.splMic();
    const int newSpl = mic ? mic->channel : -1;
    if (newSpl != splChannel) {
        splChannel = newSpl;
        spl.prepare(sampleRate);
    }

    auto channelOf = [&](int id) {
        const auto* i = c.inputs.find(id);
        return i && !i->muted ? i->channel : -1;
    };
    const int a = channelOf(c.inputs.deltaA), b = channelOf(c.inputs.deltaB);
    if (a != deltaChannelA || b != deltaChannelB) {
        deltaChannelA = a;
        deltaChannelB = b;
        if (a >= 0 && b >= 0) delta.prepare(sampleRate);
    }
    active = c;
}

void Analyser::tick()
{
    std::lock_guard<std::mutex> g(deviceLock);
    if (fifos.empty() || sampleRate <= 0) return;

    const auto gen = deviceGeneration.load();
    if (gen != seenGeneration) { // new device or rate: start everything fresh
        seenGeneration = gen;
        dsps.clear();
        splChannel = deltaChannelA = deltaChannelB = -1;
        seenControlVersion = 0;
        scratch.assign(fifos.size(), std::vector<float>(size_t(kMaxBlock)));
        fastLine.resize(size_t(grid::kPoints));
        processed = sincePublish = 0;
    }
    {
        std::lock_guard<std::mutex> c(controlLock);
        if (controlVersion != seenControlVersion) {
            seenControlVersion = controlVersion;
            applyControls(controls);
        }
    }
    if (resetRequested.exchange(false)) {
        for (auto& d : dsps) { d->spectrum.resetAveraging(); d->peaks.reset(); }
        spl.resetLeq();
    }

    // Channels pushed in the same callback have the same count; drain the common amount so A/B
    // for the delta stay sample-aligned, and flush channels nobody listens to.
    std::vector<bool> used(fifos.size(), false);
    auto use = [&](int ch) { if (ch >= 0 && ch < int(used.size())) used[size_t(ch)] = true; };
    for (auto& d : dsps) use(d->channel);
    use(splChannel);
    use(deltaChannelA);
    use(deltaChannelB);

    for (;;) {
        size_t n = size_t(kMaxBlock);
        bool any = false;
        for (size_t ch = 0; ch < fifos.size(); ++ch) {
            if (!used[ch]) { fifos[ch]->clear(); continue; }
            n = std::min(n, fifos[ch]->available());
            any = true;
        }
        if (!any || n == 0) break;
        for (size_t ch = 0; ch < fifos.size(); ++ch)
            if (used[ch]) fifos[ch]->pop(scratch[ch].data(), n);

        for (auto& d : dsps)
            if (d->channel < int(fifos.size())) d->spectrum.process(scratch[size_t(d->channel)].data(), int(n));
        if (splChannel >= 0 && splChannel < int(fifos.size())) spl.process(scratch[size_t(splChannel)].data(), int(n));
        if (deltaChannelA >= 0 && deltaChannelB >= 0 && deltaChannelA < int(fifos.size()) && deltaChannelB < int(fifos.size()))
            delta.process(scratch[size_t(deltaChannelA)].data(), scratch[size_t(deltaChannelB)].data(), int(n));

        processed += (long long)n;
        sincePublish += (long long)n;
        if (sincePublish >= (long long)(sampleRate / kPublishHz)) {
            sincePublish = 0;
            publish(active);
        }
    }
}

void Analyser::publish(const Controls& c)
{
    const double now = double(processed) / sampleRate;
    Frame& f = building;
    f.seq = ++seq;
    f.sampleRate = sampleRate;
    f.deviceChannels = int(fifos.size());
    f.traces.resize(dsps.size());
    f.bands.clear();

    const int focused = c.inputs.find(c.inputs.focusedId) ? c.inputs.focusedId : (dsps.empty() ? -1 : dsps.front()->id);
    for (size_t i = 0; i < dsps.size(); ++i) {
        auto& d = *dsps[i];
        auto& t = f.traces[i];
        t.id = d.id;
        t.live = d.channel < int(fifos.size());
        t.hasData = d.spectrum.hasData();
        d.spectrum.line(c.averaging, t.line.data());
        d.spectrum.updateHold(t.line.data());
        d.spectrum.hold(t.hold.data());
        d.spectrum.line(Averaging::Fast, fastLine.data());
        d.peaks.update(fastLine.data(), now);
        t.peaks = d.peaks.peaks();
        if (d.id == focused) {
            f.bands = d.spectrum.bands(c.averaging, c.bandsPerOctave);
            if (t.hasData && columns.space() >= size_t(grid::kPoints)) columns.push(fastLine.data(), size_t(grid::kPoints));
        }
    }

    f.deltaValid = deltaChannelA >= 0 && deltaChannelB >= 0 && delta.valid();
    if (f.deltaValid) {
        f.deltaMs = delta.delayMs();
        delta.line(f.delta.data());
    }
    f.splValid = splChannel >= 0 && splChannel < int(fifos.size());
    if (f.splValid) f.spl = spl.reading();
    f.splOffset = c.splOffset;
    frames.write(f);
}

} // namespace fs
