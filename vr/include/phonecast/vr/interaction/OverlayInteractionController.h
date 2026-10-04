#pragma once

#include "phonecast/core/input/IInputProvider.h"

#include <cstdint>

namespace phonecast::vr {

// Converts renderer-specific overlay coordinates into the normalized, top-left
// coordinate system shared by every remote-control backend.
class OverlayInteractionController {
public:
    void SetSurfaceSize(std::uint32_t width, std::uint32_t height) noexcept;
    core::PointerEvent PointerDown(float overlayX, float overlayY) noexcept;
    core::PointerEvent PointerMove(float overlayX, float overlayY) noexcept;
    core::PointerEvent PointerUp(float overlayX, float overlayY) noexcept;
    core::PointerEvent Scroll(float overlayX, float overlayY, float delta) noexcept;
    core::PointerEvent Back() noexcept;

private:
    core::PointerEvent Make(core::PointerEvent::Type type, float overlayX,
                            float overlayY, float scrollDelta = 0.0F) noexcept;

    std::uint32_t width_{1};
    std::uint32_t height_{1};
    std::uint64_t nextSequence_{};
};

}  // namespace phonecast::vr
