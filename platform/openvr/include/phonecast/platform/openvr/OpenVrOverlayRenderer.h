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
    bool TakeSettingsMenuInput(vr::SettingsMenuInput& input) override;
    void SetRemoteControlStatus(bool known, bool appEnabled,
                                bool accessibilityEnabled) override;
    void SetPairingCode(const std::string& pairCode) override;
    // Opens the SteamVR dashboard with PhoneCast selected. Used by the native
    // launcher and by second-instance IPC; it never starts SteamVR itself.
    bool FocusDashboard(std::string& error);
    bool PlaceBesideDashboard(vr::OverlaySettings& settings, std::string& error);
    std::string DefaultAudioDeviceId() const;
    bool TakePointerEvent(core::PointerEvent& event) override;
    bool ShowNotification(const core::protocol::NotificationEvent& notification,
                          std::string& error) override;
    bool HideNotification(std::string& error) override;
    bool TakeNotificationOpenRequest(std::uint64_t& actionToken) override;
    bool TakeHideRequest() override;
    bool TakeQuitRequest() override;
    bool GetPerformanceStats(vr::VrPerformanceStats& stats) override;
    void Stop() noexcept override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace phonecast::platform::openvr
