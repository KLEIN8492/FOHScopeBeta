#pragma once
#include "core/DeviceSink.h"
#include "core/Frame.h"
#include "core/Inputs.h"
#include "dsp/DeltaTrace.h"
#include "rt/TripleBuffer.h"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>

namespace fs {

// Owns the whole analysis. Threads:
//  - audio:    one per interface; each DeviceSink copies its channels into FIFOs.
//  - analysis: its own thread (tick every 10 ms) drains each sink on its own (a stalled
//              interface never holds up the others), runs all DSP, publishes a Frame (~30 Hz)
//              plus one spectrograph column per Frame.
//  - message:  set*() change the controls; latest() / popSpectrogramColumn() read results.
class Analyser : private juce::Thread
{
public:
    Analyser();
    ~Analyser() override;

    void startAnalysis() { startThread(juce::Thread::Priority::high); }
    void stopAnalysis() { stopThread(2000); }

    // The sink for an interface, created on first use (any thread but the audio threads).
    std::shared_ptr<DeviceSink> sink(const juce::String& device);
    std::shared_ptr<DeviceSink> findSink(const juce::String& device) const;

    // ---- message thread ----
    void setInputs(const InputList&);
    void setAveraging(Averaging);
    void setBandsPerOctave(int);
    void setPeakParams(PeakFinder::Params);
    void setSplOffset(double dB);
    void resetAveraging() { resetRequested.store(true); }
    const Frame& latest() { return frames.read(); }
    bool popSpectrogramColumn(float* out); // grid::kPoints values, dB re FS RMS (Fast)

    void tick(); // analysis thread (tests call it directly)

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
        int id = 0;
        PortRef port;
        double rate = 0; // 0 = not prepared yet (its interface has not started)
        MtwSpectrum spectrum;
        PeakFinder peaks;
    };
    struct SinkState
    {
        uint64_t seenGeneration = 0;
        std::vector<std::vector<float>> scratch;
    };

    void run() override;
    void applyControls(const Controls&);
    void restartDevice(const juce::String& device, double rate);
    void drain(DeviceSink&, SinkState&, size_t& advanced);
    void publish(const Controls&, double now);

    mutable std::mutex sinksLock;
    std::map<juce::String, std::shared_ptr<DeviceSink>> sinks;

    std::mutex controlLock;
    Controls controls;
    uint64_t controlVersion = 1;

    // Analysis-thread state.
    uint64_t seenControlVersion = 0;
    Controls active;
    std::vector<std::unique_ptr<InputDsp>> dsps;
    std::map<juce::String, SinkState> sinkStates;
    SplMeter spl;
    PortRef splPort;
    double splRate = 0;
    DeltaTrace delta;
    PortRef deltaA, deltaB;
    double deltaRate = 0;
    std::vector<float> fastLine;
    double nowSec = 0, sincePublish = 0;
    uint64_t seq = 0;
    std::atomic<bool> resetRequested { false };

    TripleBuffer<Frame> frames;
    SpscFifo<float> columns { 1 << 15 };
    Frame building;
};

} // namespace fs
