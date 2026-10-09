// Test source for FOHScope without hardware: plays into an output device (default "BlackHole 16ch")
//   ch1  "FOH mic":  desk L delayed 25 ms through a +6 dB / 2 kHz high shelf at −6 dB,
//                    plus a 2.5 kHz ring every 20 s (4 s swell, 6 s sustain) (feedback simulation)
//   ch2  "desk L":   pink noise
//   ch3  "desk R":   pink noise (other seed)
// Usage: FOHScopeSignalGen [device name] [seconds]
#include "PinkNoise.h"
#include "dsp/Biquad.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include <iostream>

namespace {

fs::Biquad highShelf(double f, double gainDb, double sr)
{
    const double A = std::pow(10.0, gainDb / 40.0), w0 = 2 * M_PI * f / sr, cs = std::cos(w0), sn = std::sin(w0);
    const double al = sn / 2 * std::sqrt(2.0), r = 2 * std::sqrt(A) * al, a0 = (A + 1) - (A - 1) * cs + r;
    return { A * ((A + 1) + (A - 1) * cs + r) / a0, -2 * A * ((A - 1) + (A + 1) * cs) / a0, A * ((A + 1) + (A - 1) * cs - r) / a0,
             2 * ((A - 1) - (A + 1) * cs) / a0, ((A + 1) - (A - 1) * cs - r) / a0 };
}

class Gen : public juce::AudioIODeviceCallback
{
public:
    void audioDeviceAboutToStart(juce::AudioIODevice* d) override
    {
        sr = d->getCurrentSampleRate();
        delay.assign(size_t(sr), 0.0f);
        shelf = highShelf(2000, 6, sr);
        l.setSeed(1);
        r.setSeed(2);
    }
    void audioDeviceStopped() override {}
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* out, int numOut, int n,
                                          const juce::AudioIODeviceCallbackContext&) override
    {
        const size_t d = size_t(0.025 * sr);
        for (int i = 0; i < n; ++i, ++t) {
            const float a = l.next() * 0.1f, b = r.next() * 0.1f;
            delay[pos] = a;
            const float late = delay[(pos + delay.size() - d) % delay.size()];
            pos = (pos + 1) % delay.size();
            const double sec = double(t) / sr, phase = std::fmod(sec, 20.0);
            // Swells 40 dB over 4 s, then rings steadily for 6 s.
            const double ring = phase > 6 && phase < 16 ? 0.0004 * std::pow(10.0, std::min(phase - 6, 4.0) / 4 * 2.0) : 0.0;
            const float mic = 0.5f * float(st.process(shelf, late)) + float(ring * std::sin(2 * M_PI * 2500.0 * sec));
            const float v[3] = { mic, a, b };
            for (int c = 0; c < numOut; ++c)
                if (out[c]) out[c][i] = c < 3 ? v[c] : 0.0f;
        }
    }

private:
    double sr = 48000;
    long long t = 0;
    fs::PinkNoise l, r;
    std::vector<float> delay;
    size_t pos = 0;
    fs::Biquad shelf;
    fs::BiquadState st;
};

}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String name = argc > 1 ? argv[1] : "BlackHole 16ch";
    const double seconds = argc > 2 ? std::atof(argv[2]) : 3600.0;

    juce::AudioDeviceManager dm;
    dm.initialise(0, 3, nullptr, false);
    auto setup = dm.getAudioDeviceSetup();
    setup.outputDeviceName = name;
    setup.inputDeviceName = {};
    setup.useDefaultOutputChannels = false;
    setup.outputChannels.clear();
    setup.outputChannels.setRange(0, 3, true);
    if (const auto err = dm.setAudioDeviceSetup(setup, true); err.isNotEmpty()) {
        std::cerr << "Cannot open " << name << ": " << err << "\n";
        return 1;
    }
    Gen gen;
    dm.addAudioCallback(&gen);
    std::cout << "Playing into " << name << " for " << seconds << " s (ch1 mic sim, ch2/3 pink)\n";
    juce::Thread::sleep(int(seconds * 1000));
    dm.removeAudioCallback(&gen);
    return 0;
}
