#include "phonecast/vr/interaction/OverlayInteractionController.h"

#include <algorithm>

namespace phonecast::vr {
namespace {
float ClampUnit(float value) noexcept {
    return std::max(0.0F, std::min(1.0F, value));
}
}  // namespace

void OverlayInteractionController::SetSurfaceSize(std::uint32_t width,
                                                  std::uint32_t height) noexcept {
    width_ = std::max<std::uint32_t>(1, width);
    height_ = std::max<std::uint32_t>(1, height);
}

core::PointerEvent OverlayInteractionController::Make(core::PointerEvent::Type type,
                                                       float overlayX, float overlayY,
                                                       float scrollDelta) noexcept {
    core::PointerEvent event;
    event.type = type;
    event.normalizedX = ClampUnit(overlayX / static_cast<float>(width_));
    // OpenVR overlay events have a bottom-left origin; Android uses top-left.
    event.normalizedY = ClampUnit(1.0F - overlayY / static_cast<float>(height_));
    event.scrollDelta = std::max(-1.0F, std::min(1.0F, scrollDelta));
    event.sequence = nextSequence_++;
    return event;
}

core::PointerEvent OverlayInteractionController::PointerDown(float x, float y) noexcept {
    return Make(core::PointerEvent::Type::Down, x, y);
}
core::PointerEvent OverlayInteractionController::PointerMove(float x, float y) noexcept {
    return Make(core::PointerEvent::Type::Move, x, y);
}
core::PointerEvent OverlayInteractionController::PointerUp(float x, float y) noexcept {
    return Make(core::PointerEvent::Type::Up, x, y);
}
core::PointerEvent OverlayInteractionController::Scroll(float x, float y, float delta) noexcept {
    return Make(core::PointerEvent::Type::Scroll, x, y, delta);
}
core::PointerEvent OverlayInteractionController::Back() noexcept {
    return Make(core::PointerEvent::Type::Back, 0.5F * width_, 0.5F * height_);
}

}  // namespace phonecast::vr
