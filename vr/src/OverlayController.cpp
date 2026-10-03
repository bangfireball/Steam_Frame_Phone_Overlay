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
            if (settings_.placementMode == PlacementMode::WorldLocked && settings_.worldTransformValid)
                settings_.worldTransform[3] -= 0.05F;
            else settings_.offsetXMeters = Clamp(settings_.offsetXMeters - 0.05F, -3.0F, 3.0F);
            break;
        case OverlayAction::MoveRight:
            if (settings_.placementMode == PlacementMode::WorldLocked && settings_.worldTransformValid)
                settings_.worldTransform[3] += 0.05F;
            else settings_.offsetXMeters = Clamp(settings_.offsetXMeters + 0.05F, -3.0F, 3.0F);
            break;
        case OverlayAction::MoveUp:
            if (settings_.placementMode == PlacementMode::WorldLocked && settings_.worldTransformValid)
                settings_.worldTransform[7] += 0.05F;
            else settings_.offsetYMeters = Clamp(settings_.offsetYMeters + 0.05F, -3.0F, 3.0F);
            break;
        case OverlayAction::MoveDown:
            if (settings_.placementMode == PlacementMode::WorldLocked && settings_.worldTransformValid)
                settings_.worldTransform[7] -= 0.05F;
            else settings_.offsetYMeters = Clamp(settings_.offsetYMeters - 0.05F, -3.0F, 3.0F);
            break;
        case OverlayAction::DistanceNearer:
            if (settings_.placementMode == PlacementMode::WorldLocked && settings_.worldTransformValid)
                settings_.worldTransform[11] += 0.05F;
            else if (settings_.placementMode == PlacementMode::LeftControllerLocked ||
                     settings_.placementMode == PlacementMode::RightControllerLocked)
                settings_.controllerDistanceMeters = Clamp(
                    settings_.controllerDistanceMeters - 0.02F, 0.05F, 2.0F);
            else settings_.distanceMeters = Clamp(settings_.distanceMeters - 0.05F, 0.2F, 10.0F);
            break;
        case OverlayAction::DistanceFarther:
            if (settings_.placementMode == PlacementMode::WorldLocked && settings_.worldTransformValid)
                settings_.worldTransform[11] -= 0.05F;
            else if (settings_.placementMode == PlacementMode::LeftControllerLocked ||
                     settings_.placementMode == PlacementMode::RightControllerLocked)
                settings_.controllerDistanceMeters = Clamp(
                    settings_.controllerDistanceMeters + 0.02F, 0.05F, 2.0F);
            else settings_.distanceMeters = Clamp(settings_.distanceMeters + 0.05F, 0.2F, 10.0F);
            break;
        case OverlayAction::OpacityUp:
            settings_.alpha = Clamp(settings_.alpha + 0.05F, 0.0F, 1.0F); break;
        case OverlayAction::OpacityDown:
            settings_.alpha = Clamp(settings_.alpha - 0.05F, 0.0F, 1.0F); break;
        case OverlayAction::HeadLocked:
            settings_.placementMode = PlacementMode::HeadLocked; break;
        case OverlayAction::WorldLocked:
            settings_.placementMode = PlacementMode::WorldLocked;
            settings_.worldTransformValid = false;
            break;
        case OverlayAction::LeftControllerLocked:
            settings_.placementMode = PlacementMode::LeftControllerLocked; break;
        case OverlayAction::RightControllerLocked:
            settings_.placementMode = PlacementMode::RightControllerLocked; break;
        case OverlayAction::Reset:
            settings_ = initial_;
            visible_ = true;
            break;
    }
}

}  // namespace phonecast::vr
