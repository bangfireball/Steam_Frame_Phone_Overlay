#pragma once
#include <array>
#include <cmath>

namespace phonecast::vr {
// Head-local snapshot: +Z normal points toward the user at the snapshot origin.
// Multiply by the captured HMD pose once to obtain a world-stable placement.
inline std::array<float, 12> StartLocation(float x, float y, float distance) noexcept {
    const float length = std::sqrt(x*x + y*y + distance*distance);
    const float nx = -x / length, ny = -y / length, nz = distance / length;
    const float rightLength = std::sqrt(nx*nx + nz*nz);
    const float rx = nz / rightLength, rz = -nx / rightLength;
    return {rx, ny*rz, nx, x,
            0.0F, nz*rx - nx*rz, ny, y,
            rz, -ny*rx, nz, -distance};
}
} // namespace phonecast::vr
