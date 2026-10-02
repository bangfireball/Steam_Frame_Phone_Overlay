#pragma once

#include "phonecast/core/logging/ILogger.h"
#include "phonecast/core/streaming/IVideoSource.h"
#include "phonecast/vr/overlay/IOverlayRenderer.h"

#include <cstdint>
#include <string>

namespace phonecast::apps {

class Receiver {
public:
    Receiver(core::IVideoSource& source, vr::IOverlayRenderer& renderer, core::ILogger& logger);
    bool Start(const vr::OverlaySettings& settings, std::string& error);
    // Returns false when the runtime requests shutdown or a frame fails.
    bool Tick(std::string& error);
    void Stop() noexcept;
    [[nodiscard]] std::uint64_t SubmittedFrames() const noexcept { return submittedFrames_; }

private:
    core::IVideoSource& source_;
    vr::IOverlayRenderer& renderer_;
    core::ILogger& logger_;
    bool started_{false};
    std::uint64_t submittedFrames_{0};
};

}  // namespace phonecast::apps
