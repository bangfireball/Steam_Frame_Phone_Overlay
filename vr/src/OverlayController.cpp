#include "phonecast/vr/overlay/OverlayController.h"

#include <algorithm>

namespace phonecast::vr {
namespace {
float Clamp(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(maximum, value));
}
}  // namespace

OverlayController::OverlayController(OverlaySettings initial)
    : initial_(initial), settings_(initial) {}

void OverlayController::Apply(OverlayAction action) noexcept {
    switch (action) {
        case OverlayAction::ToggleVisibility: visible_ = !visible_; break;
        case OverlayAction::ScaleUp:
            settings_.widthMeters = Clamp(settings_.widthMeters + 0.05F, 0.1F, 5.0F); break;
        case OverlayAction::ScaleDown:
            settings_.widthMeters = Clamp(settings_.widthMeters - 0.05F, 0.1F, 5.0F); break;
        case OverlayAction::MoveLeft:
            settings_.offsetXMeters = Clamp(settings_.offsetXMeters - 0.05F, -3.0F, 3.0F); break;
        case OverlayAction::MoveRight:
            settings_.offsetXMeters = Clamp(settings_.offsetXMeters + 0.05F, -3.0F, 3.0F); break;
        case OverlayAction::MoveUp:
            settings_.offsetYMeters = Clamp(settings_.offsetYMeters + 0.05F, -3.0F, 3.0F); break;
        case OverlayAction::MoveDown:
            settings_.offsetYMeters = Clamp(settings_.offsetYMeters - 0.05F, -3.0F, 3.0F); break;
        case OverlayAction::DistanceNearer:
            settings_.distanceMeters = Clamp(settings_.distanceMeters - 0.05F, 0.2F, 10.0F); break;
        case OverlayAction::DistanceFarther:
            settings_.distanceMeters = Clamp(settings_.distanceMeters + 0.05F, 0.2F, 10.0F); break;
        case OverlayAction::OpacityUp:
            settings_.alpha = Clamp(settings_.alpha + 0.05F, 0.0F, 1.0F); break;
        case OverlayAction::OpacityDown:
            settings_.alpha = Clamp(settings_.alpha - 0.05F, 0.0F, 1.0F); break;
        case OverlayAction::Reset:
            settings_ = initial_;
            visible_ = true;
            break;
    }
}

}  // namespace phonecast::vr
