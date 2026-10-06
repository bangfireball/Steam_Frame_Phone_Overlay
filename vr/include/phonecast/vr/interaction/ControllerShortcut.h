#pragma once

#include "phonecast/vr/overlay/IOverlayRenderer.h"

namespace phonecast::vr {

inline RadialMenuAction ControllerDockShortcutAction(
        GlanceInput hand, bool phoneVisible, PlacementMode placementMode) noexcept {
    const auto requestedMode = hand == GlanceInput::LeftController
        ? PlacementMode::LeftControllerLocked
        : PlacementMode::RightControllerLocked;
    if (phoneVisible && placementMode == requestedMode)
        return RadialMenuAction::PinCurrentPosition;
    return hand == GlanceInput::LeftController
        ? RadialMenuAction::LeftControllerLocked
        : RadialMenuAction::RightControllerLocked;
}

}  // namespace phonecast::vr
