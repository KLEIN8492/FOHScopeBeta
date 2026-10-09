#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace fs::grid {

// The display grid shared by every view: 1/48 octave from 20 Hz, 480 points (20 Hz … ~20.2 kHz).
constexpr int kPerOctave = 48;
constexpr int kPoints = 480;
constexpr double kF0 = 20.0;

inline double freqAt(double index) { return kF0 * std::exp2(index / kPerOctave); }
inline double indexOf(double hz) { return kPerOctave * std::log2(hz / kF0); }

// Equal-tempered note name with cents, A4 = 440 Hz: "D#7 -12c".
inline juce::String noteName(double hz)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const double midi = 69.0 + 12.0 * std::log2(hz / 440.0);
    const int nearest = int(std::lround(midi));
    const int cents = int(std::lround((midi - nearest) * 100.0));
    const int pc = ((nearest % 12) + 12) % 12, octave = nearest / 12 - 1;
    return juce::String(names[pc]) + juce::String(octave) + " " + (cents >= 0 ? "+" : "") + juce::String(cents) + "c";
}

} // namespace fs::grid
