#include "core/SpectrogramHistory.h"
#include <algorithm>
#include <cmath>

namespace fs {

namespace {
constexpr float kFloorDb = -125.0f, kStepDb = 0.5f;
}

SpectrogramHistory::SpectrogramHistory(int capacityRows)
    : cells(size_t(std::max(1, capacityRows)) * size_t(grid::kPoints), 0), capacity(std::max(1, capacityRows))
{
}

void SpectrogramHistory::clear()
{
    head = count = 0;
    total = 0;
}

uint8_t SpectrogramHistory::quantize(float db)
{
    if (!(db >= kFloorDb - kStepDb)) return 0; // also NaN
    return uint8_t(std::clamp(int(std::lround((db - kFloorDb) / kStepDb)) + 1, 1, 255));
}

float SpectrogramHistory::dequantize(uint8_t q)
{
    return q == 0 ? kFloorDb - kStepDb : kFloorDb + float(q - 1) * kStepDb;
}

void SpectrogramHistory::push(const float* col)
{
    head = (head + 1) % capacity;
    auto* dst = cells.data() + size_t(head) * size_t(grid::kPoints);
    for (int i = 0; i < grid::kPoints; ++i) dst[i] = quantize(col[i]);
    count = std::min(count + 1, capacity);
    ++total;
}

const uint8_t* SpectrogramHistory::row(int rowsAgo) const
{
    if (rowsAgo < 0 || rowsAgo >= count) return nullptr;
    const int slot = ((head - rowsAgo) % capacity + capacity) % capacity;
    return cells.data() + size_t(slot) * size_t(grid::kPoints);
}

float SpectrogramHistory::level(int rowsAgo, int i) const
{
    const auto* r = row(rowsAgo);
    return r && i >= 0 && i < grid::kPoints ? dequantize(r[i]) : -200.0f;
}

uint64_t SpectrogramHistory::clampEnd(uint64_t end, int visible) const
{
    if (total == 0) return 0;
    const uint64_t newest = total - 1, oldest = total - uint64_t(count);
    const uint64_t lowest = std::min(newest, oldest + uint64_t(std::max(1, visible)) - 1);
    return std::clamp(end, lowest, newest);
}

} // namespace fs
