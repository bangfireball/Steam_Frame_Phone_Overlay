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

} // namespace phonecast::vr
