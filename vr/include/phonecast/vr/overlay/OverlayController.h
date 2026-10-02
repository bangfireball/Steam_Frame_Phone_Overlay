#pragma once

#include "phonecast/vr/overlay/IOverlayRenderer.h"

namespace phonecast::vr {

enum class OverlayAction {
    ToggleVisibility,
    ScaleUp,
    ScaleDown,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    DistanceNearer,
    DistanceFarther,
    OpacityUp,
    OpacityDown,
    Reset
};

class OverlayController {
public:
    explicit OverlayController(OverlaySettings initial = {});

    void Apply(OverlayAction action) noexcept;
    [[nodiscard]] const OverlaySettings& Settings() const noexcept { return settings_; }
    [[nodiscard]] bool Visible() const noexcept { return visible_; }

private:
    OverlaySettings initial_;
    OverlaySettings settings_;
    bool visible_{true};
};

}  // namespace phonecast::vr
