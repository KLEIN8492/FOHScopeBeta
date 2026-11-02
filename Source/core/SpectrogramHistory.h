#pragma once
#include "dsp/FreqGrid.h"
#include <cstdint>
#include <vector>

namespace fs {

// The spectrograph's memory: the last `capacity` columns (15 min at 30 per second by default),
// independent of how many seconds are on screen, so you can scroll back to how it was. Each cell
// is a level, not a colour — one byte in 0.5 dB steps — so a history redraws correctly after the
// colour range or the units (dBFS / dB SPL) change. 15 min = 13 MB. Message thread only.
//
// Rows have absolute numbers (0 = the first row pushed since clear()): a view parked on a moment
// keeps pointing at it while new rows arrive.
class SpectrogramHistory
{
public:
    static constexpr int kRowsPerSecond = 30;
    static constexpr int kDefaultRows = 15 * 60 * kRowsPerSecond;

    explicit SpectrogramHistory(int capacityRows = kDefaultRows);

    void clear();
    void push(const float* gridDbRms); // grid::kPoints values, dB re FS RMS

    int size() const { return count; }        // rows kept
    uint64_t pushed() const { return total; }  // rows pushed since clear()
    // dB re FS RMS of grid point i, `rowsAgo` rows before the newest; −200 if not kept.
    float level(int rowsAgo, int i) const;
    const uint8_t* row(int rowsAgo) const;     // nullptr if not kept

    // Scrolling. `end` = absolute row shown at the top of a view `visible` rows tall. Clamped so
    // the view never runs past now nor past the oldest row kept (while history is shorter than
    // the view it stays at now).
    uint64_t clampEnd(uint64_t end, int visible) const;
    bool isLive(uint64_t end) const { return total == 0 || end + 1 >= total; }
    uint64_t rowsAgo(uint64_t end) const { return total == 0 ? 0 : total - 1 - end; }

    // 0 = no data / silence floor (−125.5 dB or below); 1..255 = −125 … +2 dB re FS RMS.
    static uint8_t quantize(float dbRms);
    static float dequantize(uint8_t q);

private:
    std::vector<uint8_t> cells; // capacity × grid::kPoints, ring of rows
    int capacity, head = 0, count = 0; // head = slot of the newest row
    uint64_t total = 0;
};

} // namespace fs
