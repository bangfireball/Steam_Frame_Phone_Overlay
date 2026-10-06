#pragma once

#include <algorithm>
#include <cmath>

namespace phonecast::vr {

// Positive stick Y increases distance along the head-to-panel ray, not world Y.
inline float AdjustPanelDepth(float distance, float axis, float seconds) noexcept {
    if (!std::isfinite(distance) || !std::isfinite(axis) || !std::isfinite(seconds))
        return distance;
    if (std::fabs(axis) < 0.20F || seconds <= 0.0F) return distance;
    const float delta = std::clamp(axis, -1.0F, 1.0F) * std::min(seconds, 0.05F) * 0.6F;
    return std::clamp(distance + delta, 0.20F, 3.0F);
}

// Scroll deltas are event increments, not polled axes: do not apply a deadzone
// or integrate elapsed time. Smooth-only delivery avoids double-applying runtimes
// that send both discrete and smooth events for the same stick movement.
class PanelDepthScroll {
public:
    void Add(float delta, bool smooth, bool ownerMatches) noexcept {
        if (!ownerMatches || !std::isfinite(delta)) return;
        if (smooth) ++smoothEvents;
        else ++discreteEvents;
        if (!smooth) return;
        if (delta > 0) ++positiveEvents;
        if (delta < 0) ++negativeEvents;
        pendingMeters_ = std::clamp(pendingMeters_ + std::clamp(delta, -1.0F, 1.0F) * 0.05F,
                                    -0.03F, 0.03F);
    }
    float TakeDistance(float distance) noexcept {
        const float delta = pendingMeters_;
        pendingMeters_ = 0;
        if (!std::isfinite(distance)) return distance;
        return delta == 0 ? distance : std::clamp(distance + delta, 0.20F, 3.0F);
    }
    unsigned smoothEvents{0}, discreteEvents{0}, positiveEvents{0}, negativeEvents{0};
private:
    float pendingMeters_{0};
};

} // namespace phonecast::vr
