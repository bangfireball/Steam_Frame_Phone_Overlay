#pragma once

#include "phonecast/core/logging/ILogger.h"
#include "phonecast/vr/overlay/IOverlayRenderer.h"

#include <memory>
#include <string>

namespace phonecast::platform::openvr {

class OpenVrOverlayRenderer final : public vr::IOverlayRenderer {
public:
    OpenVrOverlayRenderer(core::ILogger& logger, std::string executablePath);
    ~OpenVrOverlayRenderer() override;

    OpenVrOverlayRenderer(const OpenVrOverlayRenderer&) = delete;
    OpenVrOverlayRenderer& operator=(const OpenVrOverlayRenderer&) = delete;

    bool Start(const vr::OverlaySettings& settings, std::string& error) override;
    bool SubmitFrame(const core::VideoFrame& frame, std::string& error) override;
    bool ApplySettings(const vr::OverlaySettings& settings, std::string& error) override;
    bool SetVisible(bool visible, std::string& error) override;
    bool PumpEvents() override;
    void Stop() noexcept override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace phonecast::platform::openvr
