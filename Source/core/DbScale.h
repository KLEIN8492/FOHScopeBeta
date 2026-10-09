#pragma once
#include <algorithm>
#include <cmath>

namespace fs {

// The spectrum's dB axis: `top` (dBFS at the top edge) and `range` (dB shown). Pure arithmetic,
// shared by the mouse gestures, the Z fit and the AUTO follow.
struct DbScale
{
    static constexpr float kMinRange = 30.0f, kMaxRange = 150.0f, kMinTop = -60.0f, kMaxTop = 20.0f;

    float top = 0.0f, range = 90.0f;

    DbScale clamped() const
    {
        DbScale s = *this;
        s.range = std::clamp(s.range, kMinRange, kMaxRange);
        s.top = std::clamp(s.top, kMinTop, kMaxTop);
        return s;
    }

    DbScale panned(float db) const { return DbScale { top + db, range }.clamped(); }

    // Zoom by `factor` (> 1 shows more dB) keeping `anchorDb` at the same height on screen.
    DbScale zoomed(float factor, float anchorDb) const
    {
        const float r = std::clamp(range * factor, kMinRange, kMaxRange);
        const float frac = (top - anchorDb) / range; // 0 at the top edge, 1 at the bottom
        return DbScale { anchorDb + frac * r, r }.clamped();
    }

    // Fit: the loudest trace 6 dB under the top edge, on a 5 dB step; the range is kept.
    DbScale fittedTo(float loudestDb) const
    {
        return DbScale { std::ceil((loudestDb + 6.0f) / 5.0f) * 5.0f, range }.clamped();
    }

    // AUTO: called once per frame. Leaves the scale alone while the loudest trace sits between
    // 1 dB and 60 % of the range under the top; otherwise eases toward the fit (≤ maxStepDb).
    DbScale followed(float loudestDb, float maxStepDb = 0.3f) const
    {
        if (loudestDb <= top - 1.0f && loudestDb >= top - 0.6f * range) return *this;
        const float target = fittedTo(loudestDb).top;
        const float step = std::clamp(target - top, -maxStepDb, maxStepDb);
        return DbScale { top + step, range }.clamped();
    }
};

} // namespace fs
