#pragma once

#include "phonecast/vr/overlay/IOverlayRenderer.h"

namespace phonecast::vr {

enum class GlanceState {
    Hidden,
    Glance,
    Expanded,
    Pinned
};

enum class GlanceHand {
    Left,
    Right
};

class GlanceController {
public:
    explicit GlanceController(float previewScale = 0.55F);

    // A single controller action advances the complete fallback flow:
    // Hidden -> Glance -> Expanded -> Pinned -> Hidden.
    void Cycle(GlanceHand hand) noexcept;
    void SetHand(GlanceHand hand) noexcept { hand_ = hand; }
    void ShowGlance(GlanceHand hand) noexcept { hand_ = hand; state_ = GlanceState::Glance; }
    void ShowPinned() noexcept { state_ = GlanceState::Pinned; }
    void SetPreviewScale(float previewScale) noexcept;
    [[nodiscard]] float PreviewScale() const noexcept { return previewScale_; }
    void ToggleExpanded() noexcept;
    void Dismiss() noexcept { state_ = GlanceState::Hidden; }

    [[nodiscard]] GlanceState State() const noexcept { return state_; }
    [[nodiscard]] GlanceHand Hand() const noexcept { return hand_; }
    [[nodiscard]] bool Visible() const noexcept { return state_ != GlanceState::Hidden; }
    [[nodiscard]] OverlaySettings PresentationSettings(const OverlaySettings& base) const noexcept;

private:
    GlanceState state_{GlanceState::Hidden};
    GlanceHand hand_{GlanceHand::Left};
    float previewScale_{0.55F};
};

}  // namespace phonecast::vr
