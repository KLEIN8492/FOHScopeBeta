#include "core/Analyser.h"

namespace fs {

namespace {
constexpr int kMaxBlock = 1 << 14;
constexpr double kPublishHz = 30.0;
}

Analyser::Analyser() : juce::Thread("FOHScope analysis") { fastLine.resize(size_t(grid::kPoints)); }

Analyser::~Analyser() { stopAnalysis(); }

std::shared_ptr<DeviceSink> Analyser::sink(const juce::String& device)
{
    std::lock_guard<std::mutex> g(sinksLock);
    auto& s = sinks[device];
    if (!s) s = std::make_shared<DeviceSink>(device);
    return s;
}

std::shared_ptr<DeviceSink> Analyser::findSink(const juce::String& device) const
{
    std::lock_guard<std::mutex> g(sinksLock);
    auto it = sinks.find(device);
    return it == sinks.end() ? nullptr : it->second;
}

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
    // Keep the DSP of inputs that still exist on the same port (no averaging reset when you add,
    // rename or recolour another input); the rest start unprepared until their interface runs.
    std::vector<std::unique_ptr<InputDsp>> next;
    for (const auto& in : c.inputs.inputs) {
        if (in.muted || !in.port.valid()) continue;
        auto it = std::find_if(dsps.begin(), dsps.end(), [&](auto& d) { return d && d->id == in.id && d->port == in.port; });
        if (it != dsps.end()) {
            next.push_back(std::move(*it));
        } else {
            auto d = std::make_unique<InputDsp>();
            d->id = in.id;
            d->port = in.port;
            next.push_back(std::move(d));
        }
        next.back()->peaks.setParams(c.peak);
    }
    dsps = std::move(next);

    const auto* mic = c.inputs.splMic();
    const PortRef newSpl = mic && mic->port.valid() ? mic->port : PortRef {};
    if (newSpl != splPort) {
        splPort = newSpl;
        splRate = 0;
    }

    auto portOf = [&](int id) {
        const auto* i = c.inputs.find(id);
        return i && !i->muted ? i->port : PortRef {};
    };
    const auto a = portOf(c.inputs.deltaA), b = portOf(c.inputs.deltaB);
    if (a != deltaA || b != deltaB) {
        deltaA = a;
        deltaB = b;
        deltaRate = 0;
    }
    active = c;
}

// An interface (re)started: everything fed from it starts fresh at its rate.
void Analyser::restartDevice(const juce::String& device, double rate)
{
    for (auto& d : dsps)
        if (d->port.device == device) {
            d->spectrum.prepare(rate);
            d->peaks.reset();
            d->rate = rate;
        }
    if (splPort.device == device) {
        spl.prepare(rate);
        splRate = rate;
    }
    if (deltaA.valid() && deltaB.valid() && (deltaA.device == device || deltaB.device == device)) {
        delta.prepare(rate);
        deltaRate = rate;
    }
}

void Analyser::drain(DeviceSink& s, SinkState& st, size_t& advanced)
{
    std::lock_guard<std::mutex> g(s.lock);
    if (s.fifos.empty() || s.rate.load() <= 0) return;
    const auto& name = s.name();
    const double rate = s.rate.load();

    if (s.gen.load() != st.seenGeneration) {
        st.seenGeneration = s.gen.load();
        st.scratch.assign(s.fifos.size(), std::vector<float>(size_t(kMaxBlock)));
        restartDevice(name, rate);
    }
    // Inputs added since the last (re)start get prepared on their first data.
    for (auto& d : dsps)
        if (d->port.device == name && d->rate != rate) { d->spectrum.prepare(rate); d->rate = rate; }
    if (splPort.device == name && splRate != rate) { spl.prepare(rate); splRate = rate; }
    const bool deltaHere = deltaA.valid() && deltaB.valid() && (deltaA.device == name || deltaB.device == name);
    if (deltaHere && deltaRate == 0) { delta.prepare(rate); deltaRate = rate; }

    const int nCh = int(s.fifos.size());
    std::vector<bool> used(size_t(nCh), false);
    auto use = [&](const PortRef& p) { if (p.device == name && p.channel < nCh) used[size_t(p.channel)] = true; };
    for (auto& d : dsps) use(d->port);
    use(splPort);
    if (deltaHere) { use(deltaA); use(deltaB); }

    size_t total = 0;
    for (;;) {
        // Channels of one interface arrive in the same callback: drain the common amount so
        // they stay sample-aligned; flush channels nobody listens to.
        size_t n = size_t(kMaxBlock);
        bool any = false;
        for (int ch = 0; ch < nCh; ++ch) {
            if (!used[size_t(ch)]) { s.fifos[size_t(ch)]->clear(); continue; }
            n = std::min(n, s.fifos[size_t(ch)]->available());
            any = true;
        }
        if (!any || n == 0) break;
        for (int ch = 0; ch < nCh; ++ch)
            if (used[size_t(ch)]) s.fifos[size_t(ch)]->pop(st.scratch[size_t(ch)].data(), n);
        auto data = [&](const PortRef& p) { return st.scratch[size_t(p.channel)].data(); };

        for (auto& d : dsps)
            if (d->port.device == name && d->port.channel < nCh) d->spectrum.process(data(d->port), int(n));
        if (splPort.device == name && splPort.channel < nCh) spl.process(data(splPort), int(n));
        if (deltaHere && deltaRate == rate) {
            if (deltaA.device == name && deltaA.channel < nCh) delta.pushA(data(deltaA), int(n));
            if (deltaB.device == name && deltaB.channel < nCh) delta.pushB(data(deltaB), int(n));
        }
        total += n;
    }
    advanced = std::max(advanced, total);
}

void Analyser::tick()
{
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

    std::vector<std::shared_ptr<DeviceSink>> list;
    {
        std::lock_guard<std::mutex> g(sinksLock);
        for (auto& [name, s] : sinks) list.push_back(s);
    }
    // Time advances with the busiest interface; all interfaces run at the shared rate.
    size_t advanced = 0;
    double rate = 0;
    for (auto& s : list) {
        size_t a = 0;
        drain(*s, sinkStates[s->name()], a);
        if (a > advanced) { advanced = a; rate = s->sampleRate(); }
    }
    if (deltaRate > 0) delta.maybeEstimate();
    if (advanced == 0 || rate <= 0) return;

    nowSec += double(advanced) / rate;
    sincePublish += double(advanced) / rate;
    if (sincePublish >= 1.0 / kPublishHz) {
        sincePublish = 0;
        publish(active, nowSec);
    }
}

void Analyser::publish(const Controls& c, double now)
{
    Frame& f = building;
    f.seq = ++seq;
    f.traces.resize(dsps.size());
    f.bands.clear();

    auto liveFor = [&](const PortRef& p) {
        auto s = findSink(p.device);
        return s && s->isRunning() && p.channel < s->numChannels();
    };
    const int focused = c.inputs.find(c.inputs.focusedId) ? c.inputs.focusedId : (dsps.empty() ? -1 : dsps.front()->id);
    for (size_t i = 0; i < dsps.size(); ++i) {
        auto& d = *dsps[i];
        auto& t = f.traces[i];
        t.id = d.id;
        t.live = liveFor(d.port);
        t.hasData = d.rate > 0 && d.spectrum.hasData();
        if (!t.hasData) { t.peaks.clear(); continue; }
        d.spectrum.line(c.averaging, t.line.data());
        d.spectrum.updateHold(t.line.data());
        d.spectrum.hold(t.hold.data());
        d.spectrum.line(Averaging::Fast, fastLine.data());
        d.peaks.update(fastLine.data(), now);
        t.peaks = d.peaks.peaks();
        if (d.id == focused) {
            f.bands = d.spectrum.bands(c.averaging, c.bandsPerOctave);
            if (columns.space() >= size_t(grid::kPoints)) columns.push(fastLine.data(), size_t(grid::kPoints));
        }
    }

    f.deltaValid = deltaRate > 0 && delta.valid();
    if (f.deltaValid) {
        f.deltaMs = delta.delayMs();
        delta.line(f.delta.data());
    }
    f.splValid = splRate > 0 && liveFor(splPort);
    if (f.splValid) f.spl = spl.reading();
    f.splOffset = c.splOffset;
    frames.write(f);
}

} // namespace fs
