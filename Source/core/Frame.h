#pragma once
#include "dsp/FreqGrid.h"
#include "dsp/MtwSpectrum.h"
#include "dsp/PeakFinder.h"
#include "dsp/SplMeter.h"
#include <array>
#include <vector>

namespace fs {

using GridLine = std::array<float, grid::kPoints>;

struct TraceFrame
{
    int id = 0;
    bool live = false;        // its interface is running and has its channel
    bool hasData = false;     // all FFT windows have produced a frame
    GridLine line {}, hold {}; // dB re FS RMS per 1/48 octave
    std::vector<Peak> peaks;
};

// One snapshot of everything the UI draws, published ~30 times a second.
struct Frame
{
    uint64_t seq = 0;
    std::vector<TraceFrame> traces;   // in InputList order, muted inputs omitted
    std::vector<Band> bands;          // focused input
    bool deltaValid = false;
    double deltaMs = 0;
    GridLine delta {};
    bool splValid = false;
    SplMeter::Reading spl;            // dB re FS RMS; add splOffset for dB SPL
    double splOffset = 0;

    const TraceFrame* trace(int id) const
    {
        for (auto& t : traces) if (t.id == id) return &t;
        return nullptr;
    }
};

} // namespace fs
