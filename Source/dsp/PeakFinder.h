#pragma once
#include "dsp/FreqGrid.h"
#include <vector>

namespace fs {

struct Peak
{
    float hz = 0;
    juce::String note;
    float prominenceDb = 0; // above the 1-octave median around it
    float levelDb = -200;
    float ageSec = 0;       // how long it has stood out
};

// Finds narrow peaks that keep standing out of the spectrum: ringing / feedback candidates.
// Feed it a Fast-averaged grid line ~30 times a second. Analysis thread only.
class PeakFinder
{
public:
    struct Params
    {
        float thresholdDb = 8.0f; // prominence above the local median
        float holdSec = 1.5f;     // must stand out this long before it is reported
    };

    // One tracked peak across updates.
    struct Track
    {
        double index = 0;        // fractional grid index
        double firstSeen = 0, lastSeen = 0;
        float prominence = 0, firstProminence = 0, maxProminence = 0;
        float level = -200;
    };

    void setParams(Params p) { params = p; }
    Params getParams() const { return params; }
    void update(const float* fastLine, double nowSec);
    const std::vector<Peak>& peaks() const { return reported; }
    void reset() { tracks.clear(); reported.clear(); }

    // The rule that turns a tracked peak into a reported feedback candidate.
    static bool shouldReport(const Track& t, double nowSec, const Params& p);

private:
    Params params;
    std::vector<Track> tracks;
    std::vector<Peak> reported;
    std::vector<float> baseline, scratch;
};

} // namespace fs
