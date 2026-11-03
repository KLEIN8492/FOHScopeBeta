#pragma once
#include <juce_core/juce_core.h>

namespace fs {

// A sliding Leq that has averaged less than its window (a "15 min" Leq two minutes after start)
// is still warming up: the SPL panel says so, and the noise-limit log leaves it out.
struct LeqWarmup
{
    bool full = false;
    juce::String text; // "2:30 / 15:00" while warming, empty when full
};

// Full once the window is covered: a full sliding window averages the last T−1 whole seconds
// plus the current partial one, so it reads between T−1 and T seconds.
inline LeqWarmup leqWarmup(double coveredSeconds, int windowSeconds)
{
    if (coveredSeconds >= double(windowSeconds) - 1.0) return { true, {} };
    auto mmss = [](int s) { return juce::String(s / 60) + ":" + juce::String(s % 60).paddedLeft('0', 2); };
    return { false, mmss(int(coveredSeconds)) + " / " + mmss(windowSeconds) };
}

// What the log records for a sliding Leq: the value once its window is full, else nothing
// (−200: LevelLogger writes an empty field), so no 15-minute figure is built from seconds.
inline double leqForLog(double value, double coveredSeconds, int windowSeconds)
{
    return leqWarmup(coveredSeconds, windowSeconds).full ? value : -200.0;
}

} // namespace fs
