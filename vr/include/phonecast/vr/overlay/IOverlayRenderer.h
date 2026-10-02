#pragma once

#include "phonecast/core/streaming/VideoFrame.h"

#include <string>

namespace phonecast::vr {

struct OverlaySettings {
    float widthMeters{0.65F};
    float distanceMeters{1.0F};
    float alpha{1.0F};
};

class IOverlayRenderer {
public:
    virtual ~IOverlayRenderer() = default;
    virtual bool Start(const OverlaySettings& settings, std::string& error) = 0;
    virtual bool SubmitFrame(const core::VideoFrame& frame, std::string& error) = 0;
    // Returns false when the VR runtime asks the receiver to exit.
    virtual bool PumpEvents() = 0;
    virtual void Stop() noexcept = 0;
};

}  // namespace phonecast::vr
