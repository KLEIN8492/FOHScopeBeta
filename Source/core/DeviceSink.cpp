#include "core/DeviceSink.h"

namespace fs {

namespace {
constexpr size_t kFifoSize = 1 << 17; // ≈ 2.7 s at 48 kHz: the analysis thread may stall that long
}

void DeviceSink::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    std::vector<int> map;
    const auto activeIn = device->getActiveInputChannels();
    for (int ch = 0; ch <= activeIn.getHighestBit(); ++ch)
        if (activeIn[ch]) map.push_back(ch);
    prepare(device->getCurrentSampleRate(), device->getInputChannelNames().size(), std::move(map));
}

void DeviceSink::prepare(double sr, int numChannels, std::vector<int> map)
{
    std::lock_guard<std::mutex> g(lock);
    running.store(false);
    activeToDevice = std::move(map);
    fifos.clear();
    for (int ch = 0; ch < numChannels; ++ch) fifos.push_back(std::make_unique<SpscFifo<float>>(kFifoSize));
    meters = std::make_unique<std::atomic<float>[]>(size_t(numChannels));
    for (int ch = 0; ch < numChannels; ++ch) meters[size_t(ch)].store(0.0f);
    rate.store(sr);
    channels.store(numChannels);
    gen.fetch_add(1);
    running.store(true);
}

void DeviceSink::audioDeviceIOCallbackWithContext(const float* const* in, int numIn, float* const* out, int numOut, int n,
                                                  const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c)
        if (out[c] != nullptr) juce::FloatVectorOperations::clear(out[c], n);
    beats.fetch_add(1, std::memory_order_relaxed);
    if (!running.load(std::memory_order_acquire)) return;
    for (int c = 0; c < numIn && c < int(activeToDevice.size()); ++c) {
        const int ch = activeToDevice[size_t(c)];
        if (in[c] == nullptr || ch >= int(fifos.size())) continue;
        fifos[size_t(ch)]->push(in[c], size_t(n)); // full → drop
        const auto range = juce::FloatVectorOperations::findMinAndMax(in[c], n);
        const float peak = std::max(-range.getStart(), range.getEnd());
        auto& m = meters[size_t(ch)];
        if (peak > m.load(std::memory_order_relaxed)) m.store(peak, std::memory_order_relaxed);
    }
}

float DeviceSink::takePeak(int ch)
{
    std::lock_guard<std::mutex> g(lock);
    if (ch < 0 || ch >= channels.load() || !meters) return 0.0f;
    return meters[size_t(ch)].exchange(0.0f, std::memory_order_relaxed);
}

void DeviceSink::prepareForTest(double sr, int numChannels)
{
    std::vector<int> map(static_cast<size_t>(numChannels));
    for (int i = 0; i < numChannels; ++i) map[size_t(i)] = i;
    prepare(sr, numChannels, std::move(map));
}

void DeviceSink::pushTestBlock(const float* const* ch, int numChannels, int n)
{
    float* none[1] = { nullptr };
    audioDeviceIOCallbackWithContext(ch, numChannels, none, 0, n, {});
}

} // namespace fs
