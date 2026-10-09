#pragma once
#include "core/Frame.h"
#include "core/Inputs.h"
#include "dsp/DeltaTrace.h"
#include "rt/SpscFifo.h"
#include "rt/TripleBuffer.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <memory>
#include <mutex>

namespace fs {

// Owns the whole analysis. Three threads touch it:
//  - audio:    audioDeviceIOCallbackWithContext copies each device input channel into its FIFO.
//              No locks, no allocation.
//  - analysis: its own thread (tick every 10 ms) drains the FIFOs, runs all DSP and publishes
//              a Frame (~30 Hz) plus one spectrograph column per Frame.
//  - message:  set*() change the controls (mutex, never held by the audio thread);
//              latest() / popSpectrogramColumn() read the results (lock-free).
class Analyser : public juce::AudioIODeviceCallback, private juce::Thread
{
public:
    Analyser();
    ~Analyser() override;

    void startAnalysis() { startThread(juce::Thread::Priority::high); }
    void stopAnalysis() { stopThread(2000); }

    // ---- message thread ----
    void setInputs(const InputList&);
    void setAveraging(Averaging);
    void setBandsPerOctave(int);
    void setPeakParams(PeakFinder::Params);
    void setSplOffset(double dB);
    void resetAveraging() { resetRequested.store(true); }
    const Frame& latest() { return frames.read(); }
    bool popSpectrogramColumn(float* out); // grid::kPoints values, dB re FS RMS (Fast)
    // Routing meters: every device input channel, analysed or not. Linear peak since the last
    // call (the hold resets on read). Message thread, like the device open/close it follows.
    int numDeviceChannels() const { return meterCount.load(); }
    float takeChannelPeak(int deviceChannel);

    // ---- audio device ----
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceIOCallbackWithContext(const float* const* in, int numIn, float* const* out, int numOut, int n,
                                          const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceStopped() override;

    // ---- tests: drive it without a device or thread ----
    void prepareForTest(double sampleRate, int deviceChannels);
    void pushTestBlock(const float* const* channels, int numChannels, int n);
    void tick();

private:
    struct Controls
    {
        InputList inputs;
        Averaging averaging = Averaging::Slow;
        int bandsPerOctave = 3;
        PeakFinder::Params peak;
        double splOffset = 0;
    };
    struct InputDsp
    {
        int id = 0, channel = -1;
        MtwSpectrum spectrum;
        PeakFinder peaks;
    };

    void run() override;
    void prepareDevice(double sampleRate, int deviceChannels, std::vector<int> activeToDevice);
    void applyControls(const Controls&);
    void publish(const Controls&);

    // Device side (written in prepareDevice under deviceLock, read lock-free by the callback).
    std::mutex deviceLock;
    std::vector<std::unique_ptr<SpscFifo<float>>> fifos; // per device input channel
    std::vector<int> activeToDevice;
    std::atomic<bool> deviceActive { false };
    std::unique_ptr<std::atomic<float>[]> meters; // per device channel, written by the callback
    std::atomic<int> meterCount { 0 };
    std::atomic<uint64_t> deviceGeneration { 0 };
    double sampleRate = 0;
    int deviceChannels = 0;

    // Controls (message → analysis).
    std::mutex controlLock;
    Controls controls;
    uint64_t controlVersion = 1;

    // Analysis-thread state.
    uint64_t seenGeneration = 0, seenControlVersion = 0;
    Controls active;
    std::vector<std::unique_ptr<InputDsp>> dsps;
    SplMeter spl;
    int splChannel = -1;
    DeltaTrace delta;
    int deltaChannelA = -1, deltaChannelB = -1;
    std::vector<std::vector<float>> scratch;
    std::vector<float> fastLine;
    long long processed = 0, sincePublish = 0;
    uint64_t seq = 0;
    std::atomic<bool> resetRequested { false };

    // Results (analysis → message).
    TripleBuffer<Frame> frames;
    SpscFifo<float> columns { 1 << 15 };
    Frame building;
};

} // namespace fs
