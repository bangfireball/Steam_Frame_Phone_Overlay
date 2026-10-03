#pragma once

#include "phonecast/core/streaming/VideoFrame.h"

#include <array>
#include <string>

namespace phonecast::vr {

enum class PlacementMode {
    HeadLocked,
    WorldLocked,
    LeftControllerLocked,
    RightControllerLocked
};

struct OverlaySettings {
    float widthMeters{0.65F};
    float distanceMeters{1.0F};
    float controllerDistanceMeters{0.18F};
    float alpha{1.0F};
    float offsetXMeters{0.0F};
    float offsetYMeters{0.0F};
    PlacementMode placementMode{PlacementMode::HeadLocked};
    // Row-major 3x4 standing-space transform. It is populated when a world
    // anchor is first created or when the user releases a controller grab.
    std::array<float, 12> worldTransform{
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 1.2F,
        0.0F, 0.0F, 1.0F, -1.0F};
    bool worldTransformValid{false};
};

class IOverlayRenderer {
public:
    virtual ~IOverlayRenderer() = default;
    virtual bool Start(const OverlaySettings& settings, std::string& error) = 0;
    virtual bool SubmitFrame(const core::VideoFrame& frame, std::string& error) = 0;
    virtual bool ApplySettings(const OverlaySettings& settings, std::string& error) = 0;
    virtual bool SetVisible(bool visible, std::string& error) = 0;
    // Returns false when the VR runtime asks the receiver to exit.
    virtual bool PumpEvents() = 0;
    // Reports placement changed directly in VR (for example after a grab).
    virtual bool TakeSettingsUpdate(OverlaySettings& settings) { (void)settings; return false; }
    virtual void Stop() noexcept = 0;
};

}  // namespace phonecast::vr
