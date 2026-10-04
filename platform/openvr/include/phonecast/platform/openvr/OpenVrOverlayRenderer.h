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
    bool TakeSettingsUpdate(vr::OverlaySettings& settings) override;
    bool TakeGlanceInput(vr::GlanceInput& input) override;
    bool TakeRadialMenuSelection(vr::RadialMenuSelection& selection) override;
    bool ShowSettingsMenu(const vr::SettingsMenuView& view, std::string& error) override;
    bool HideSettingsMenu(std::string& error) override;
    bool TakeSettingsMenuInput(vr::SettingsMenuCommand& command) override;
    bool TakePointerEvent(core::PointerEvent& event) override;
    void Stop() noexcept override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace phonecast::platform::openvr
