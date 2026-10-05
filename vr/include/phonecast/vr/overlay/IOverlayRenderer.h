#pragma once

#include "phonecast/core/input/IInputProvider.h"
#include "phonecast/core/protocol/StreamProtocol.h"
#include "phonecast/core/streaming/VideoFrame.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace phonecast::vr {

enum class PlacementMode {
    HeadLocked,
    WorldLocked,
    LeftControllerLocked,
    RightControllerLocked
};

enum class ControllerOrientation {
    ControllerRelative,
    FaceUser,
    WorldUpright,
    Wrist
};

enum class GlanceInput {
    LeftController,
    RightController
};

enum class RadialMenuAction {
    ToggleVisible,
    ShowGlance,
    ShowPinned,
    HeadLocked,
    WorldLocked,
    LeftControllerLocked,
    RightControllerLocked,
    OpenSettings
};

struct RadialMenuSelection {
    RadialMenuAction action{RadialMenuAction::ToggleVisible};
    GlanceInput hand{GlanceInput::LeftController};
};

enum class SettingsMenuCommand {
    PreviousItem,
    NextItem,
    Decrease,
    Increase,
    SetNormalized,
    Activate,
    Back
};

struct SettingsMenuInput {
    SettingsMenuCommand command{SettingsMenuCommand::Activate};
    float normalizedValue{0.0F};
};

struct SettingsMenuView {
    std::string title;
    std::vector<std::string> labels;
    std::vector<std::string> values;
    // A value in [0, 1] draws an interactive slider. Negative values indicate
    // an action, category, or discrete value controlled only by -/+.
    std::vector<float> normalizedValues;
    bool showNotificationPreview{false};
    std::size_t selectedIndex{0};
};

struct ControllerCalibration {
    float distanceMeters{0.18F};
    float heightMeters{0.10F};
    float lateralMeters{0.0F};
    float tiltDegrees{0.0F};
    float yawDegrees{0.0F};
    float scale{1.0F};
    ControllerOrientation orientation{ControllerOrientation::FaceUser};
};

struct VrPerformanceStats {
    bool available{false};
    std::uint32_t frameIndex{};
    std::uint32_t framePresents{};
    std::uint32_t misPresentedFrames{};
    std::uint32_t droppedFrames{};
    std::uint32_t reprojectionFlags{};
    float totalRenderGpuMilliseconds{};
    float compositorGpuMilliseconds{};
    float compositorCpuMilliseconds{};
    float clientFrameIntervalMilliseconds{};
};

struct OverlaySettings {
    float widthMeters{0.65F};
    float distanceMeters{1.0F};
    float alpha{1.0F};
    float offsetXMeters{0.0F};
    float offsetYMeters{0.0F};
    PlacementMode placementMode{PlacementMode::HeadLocked};
    ControllerCalibration leftController{};
    ControllerCalibration rightController{};
    // Row-major 3x4 standing-space transform. It is populated when a world
    // anchor is first created or when the user releases a controller grab.
    std::array<float, 12> worldTransform{
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 1.2F,
        0.0F, 0.0F, 1.0F, -1.0F};
    bool worldTransformValid{false};
    float glancePreviewScale{0.55F};
    std::uint32_t radialLongPressMilliseconds{600};
    float notificationWidthMeters{0.55F};
    float notificationDistanceMeters{0.75F};
    float notificationOffsetXMeters{0.18F};
    float notificationOffsetYMeters{0.12F};
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
    // Reports a controller reveal/cycle action without putting OpenVR types in the app.
    virtual bool TakeGlanceInput(GlanceInput& input) { (void)input; return false; }
    // Reports a selection from the optional controller radial menu.
    virtual bool TakeRadialMenuSelection(RadialMenuSelection& selection) {
        (void)selection;
        return false;
    }
    virtual bool ShowSettingsMenu(const SettingsMenuView& view, std::string& error) {
        (void)view;
        error = "The renderer does not support an in-headset settings menu.";
        return false;
    }
    virtual bool HideSettingsMenu(std::string& error) { error.clear(); return true; }
    virtual bool TakeSettingsMenuInput(SettingsMenuInput& input) {
        (void)input;
        return false;
    }
    // Reports whether both Android remote-control consent gates are open. The
    // renderer may surface this in its dashboard without owning protocol state.
    virtual void SetRemoteControlStatus(bool known, bool appEnabled,
                                        bool accessibilityEnabled) {
        (void)known;
        (void)appEnabled;
        (void)accessibilityEnabled;
    }
    // Supplies the receiver's persisted first-use credential for local
    // in-headset display. Renderers must not write it to logs.
    virtual void SetPairingCode(const std::string& pairCode) { (void)pairCode; }
    // Reports normalized phone interaction without exposing renderer-specific coordinates.
    virtual bool TakePointerEvent(core::PointerEvent& event) {
        (void)event;
        return false;
    }
    // Displays a short-lived privacy-filtered notification card independently
    // of the full phone overlay.
    virtual bool ShowNotification(const core::protocol::NotificationEvent& notification,
                                  std::string& error) {
        (void)notification;
        error = "The renderer does not support notification cards.";
        return false;
    }
    virtual bool HideNotification(std::string& error) { error.clear(); return true; }
    // A card can request the full phone view and its Android launch action while
    // the dashboard laser is active.
    virtual bool TakeNotificationOpenRequest(std::uint64_t& actionToken) {
        (void)actionToken;
        return false;
    }
    // Reports the phone footer's local close action without sending it to Android.
    virtual bool TakeHideRequest() { return false; }
    // Reports an explicit dashboard Quit selection. Hiding UI does not quit.
    virtual bool TakeQuitRequest() { return false; }
    // Samples scene/compositor timing where the active VR backend exposes it.
    // These values describe the VR compositor's latest frame, not PhoneCast's
    // own D3D upload in isolation.
    virtual bool GetPerformanceStats(VrPerformanceStats& stats) {
        stats = {};
        return false;
    }
    virtual void Stop() noexcept = 0;
};

}  // namespace phonecast::vr
