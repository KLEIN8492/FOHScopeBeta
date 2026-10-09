#pragma once
#include "core/Analyser.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <map>
#include <optional>
#include <set>

namespace fs {

// Interfaces to keep open: those any shown input is patched to, plus every interface while the
// routing page is on screen (so its channel meters can show where the signal is).
std::set<juce::String> wantedDevices(const InputList&, bool routingVisible, const juce::StringArray& available);

// Owns every audio interface FOHScope uses (LiveRack's model). Each wanted interface is opened
// on its own (input-only, all channels, the shared rate and buffer, no aggregate / combiner) and
// started on its Analyser sink. All opening, closing and probing runs on the hub's own thread:
// a slow driver never blocks the window. A watchdog reopens interfaces that stop delivering or
// come back after being unplugged, with back-off.
class DeviceHub : private juce::Thread, private juce::AudioIODeviceType::Listener, private juce::AsyncUpdater
{
public:
    enum class State { closed, opening, open, error, missing };
    struct Info
    {
        juce::String name;
        juce::StringArray inputs; // channel names (empty until probed)
        State state = State::closed;
        juce::String error;
        int restarts = 0;
        bool wanted = false;
    };

    explicit DeviceHub(Analyser&);
    ~DeviceHub() override;

    void start() { startThread(); }
    void setFormat(double sampleRate, int bufferSize); // reopens what is open
    void setWanted(const std::set<juce::String>&);
    void setOpeningAllowed(bool); // false until microphone access is settled

    // Message thread.
    std::vector<Info> snapshot() const; // every input interface on the Mac + wanted missing ones
    std::optional<Info> find(const juce::String& name) const;
    juce::StringArray availableNames() const;
    double sampleRate() const;
    int bufferSize() const;
    std::function<void()> onChanged; // states or catalogue changed (message thread)

    static int retryDelayMs(int failures); // 1, 2, 4, 8, 16, 30 s

private:
    struct Open
    {
        std::unique_ptr<juce::AudioIODevice> device;
        std::shared_ptr<DeviceSink> sink;
        uint64_t lastBeat = 0;
        int stalledMs = 0;
    };
    struct Retry
    {
        int failures = 0;
        juce::uint32 due = 0;
    };

    void run() override;
    void audioDeviceListChanged() override;
    void handleAsyncUpdate() override;

    void rescan();
    void reconcile();
    void watchdog();
    bool openOne(const juce::String& name, const juce::StringArray& inputs, double rate, int buffer);
    void closeOne(const juce::String& name);
    void setState(const juce::String& name, State, const juce::String& error = {});
    void scheduleRetry(const juce::String& name);

    Analyser& analyser;
    std::unique_ptr<juce::AudioIODeviceType> type;

    mutable std::mutex lock; // everything below up to `open`
    std::vector<std::pair<juce::String, juce::StringArray>> available;
    std::map<juce::String, Info> states;
    std::set<juce::String> wanted;
    double rate = 48000;
    int buffer = 256;
    uint64_t formatGeneration = 0;
    bool openingAllowed = false, rescanNeeded = true;

    // Hub thread only.
    std::map<juce::String, Open> open;
    std::map<juce::String, Retry> retries;
    uint64_t openFormatGeneration = 0;
};

} // namespace fs
