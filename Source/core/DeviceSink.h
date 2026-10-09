#pragma once
#include "rt/SpscFifo.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <memory>
#include <mutex>

namespace fs {

// The landing point of one audio interface: its callback copies every input channel into a
// FIFO and keeps a peak per channel for the routing meters. One per interface, created by the
// Analyser, started by the DeviceHub. The audio callback never locks or allocates.
class DeviceSink : public juce::AudioIODeviceCallback
{
public:
    explicit DeviceSink(juce::String deviceName) : deviceName(std::move(deviceName)) {}

    const juce::String& name() const { return deviceName; }
    double sampleRate() const { return rate.load(); }
    int numChannels() const { return channels.load(); }
    bool isRunning() const { return running.load(); }
    uint64_t heartbeat() const { return beats.load(); }   // callbacks so far (watchdog)
    uint64_t generation() const { return gen.load(); }    // bumps on every (re)start
    float takePeak(int channel);                          // linear peak since the last call

    // Tests: behave as if a device with `channels` inputs started at `sr`.
    void prepareForTest(double sr, int channels);
    void pushTestBlock(const float* const* channelData, int numChannels, int n);
    void stopForTest() { running.store(false); }

    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceIOCallbackWithContext(const float* const* in, int numIn, float* const* out, int numOut, int n,
                                          const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceStopped() override { running.store(false); }

private:
    friend class Analyser;
    void prepare(double sr, int channels, std::vector<int> activeToDevice);

    const juce::String deviceName;
    std::mutex lock; // prepare() vs the analysis thread draining vs meters; never the callback
    std::vector<std::unique_ptr<SpscFifo<float>>> fifos; // per device input channel
    std::vector<int> activeToDevice;
    std::unique_ptr<std::atomic<float>[]> meters;
    std::atomic<int> channels { 0 };
    std::atomic<double> rate { 0 };
    std::atomic<bool> running { false };
    std::atomic<uint64_t> beats { 0 }, gen { 0 };
};

} // namespace fs
