#include "phonecast/vr/overlay/GlanceController.h"

#include <algorithm>

namespace phonecast::vr {

GlanceController::GlanceController(float previewScale)
    : previewScale_(std::max(0.25F, std::min(1.0F, previewScale))) {}

void GlanceController::Cycle(GlanceHand hand) noexcept {
    hand_ = hand;
    switch (state_) {
        case GlanceState::Hidden: state_ = GlanceState::Glance; break;
        case GlanceState::Glance: state_ = GlanceState::Expanded; break;
        case GlanceState::Expanded: state_ = GlanceState::Pinned; break;
        case GlanceState::Pinned: state_ = GlanceState::Hidden; break;
    }
}

void GlanceController::ToggleExpanded() noexcept {
    state_ = state_ == GlanceState::Hidden ? GlanceState::Expanded : GlanceState::Hidden;
}

OverlaySettings GlanceController::PresentationSettings(const OverlaySettings& base) const noexcept {
    auto presented = base;
    if (state_ != GlanceState::Glance) return presented;
    presented.placementMode = hand_ == GlanceHand::Left
        ? PlacementMode::LeftControllerLocked
        : PlacementMode::RightControllerLocked;
    presented.widthMeters *= previewScale_;
    return presented;
}

}  // namespace phonecast::vr
