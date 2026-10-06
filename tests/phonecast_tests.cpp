#include "phonecast/apps/Receiver.h"
#include "phonecast/core/config/AppConfig.h"
#include "phonecast/core/protocol/StreamProtocol.h"
#include "phonecast/core/streaming/DecoderRecoveryController.h"
#include "phonecast/core/streaming/GeneratedVideoSource.h"
#include "phonecast/vr/interaction/OverlayInteractionController.h"
#include "phonecast/vr/interaction/PanelDepth.h"
#include "phonecast/vr/overlay/GlanceController.h"
#include "phonecast/vr/overlay/OverlayController.h"
#include "phonecast/vr/overlay/OverlaySettingsStore.h"
#include "phonecast/vr/overlay/SettingsMenuController.h"
#include "phonecast/vr/overlay/WristMenuGesture.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

class FakeLogger final : public phonecast::core::ILogger {
public:
    void Log(phonecast::core::LogLevel, std::string_view, std::string_view) override { ++entries; }
    int entries{0};
};

class FakeOverlay final : public phonecast::vr::IOverlayRenderer {
public:
    bool Start(const phonecast::vr::OverlaySettings&, std::string&) override {
        started = true;
        return startResult;
    }
    bool SubmitFrame(const phonecast::core::VideoFrame& frame, std::string&) override {
        ++frames;
        lastSequence = frame.sequence;
        return submitResult;
    }
    bool ApplySettings(const phonecast::vr::OverlaySettings&, std::string&) override {
        return true;
    }
    bool SetVisible(bool, std::string&) override { return true; }
    bool PumpEvents() override { return keepRunning; }
    void Stop() noexcept override { stopped = true; }
    bool startResult{true};
    bool submitResult{true};
    bool keepRunning{true};
    bool started{false};
    bool stopped{false};
    int frames{0};
    std::uint64_t lastSequence{0};
};

int failures = 0;
void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void TestConfig() {
    const auto defaults = phonecast::core::ParseCommandLine({});
    Check(defaults.status == phonecast::core::ParseStatus::Success, "default config parses");
    Check(defaults.config.framesPerSecond == 30, "default FPS is 30");

    const auto custom = phonecast::core::ParseCommandLine(
        {"--width", "800", "--height", "600", "--fps", "60", "--alpha", "0.5"});
    Check(custom.status == phonecast::core::ParseStatus::Success, "custom config parses");
    Check(custom.config.textureWidth == 800 && custom.config.textureHeight == 600,
          "custom dimensions apply");
    Check(custom.config.framesPerSecond == 60 && custom.config.overlayAlpha == 0.5F,
          "custom FPS and alpha apply");
    Check(phonecast::core::ParseCommandLine({"--fps", "0"}).status ==
              phonecast::core::ParseStatus::Error,
          "invalid FPS rejected");
    Check(phonecast::core::ParseCommandLine({"--unknown", "1"}).status ==
              phonecast::core::ParseStatus::Error,
          "unknown option rejected");
    Check(phonecast::core::ParseCommandLine({"--alpha", "nan"}).status ==
              phonecast::core::ParseStatus::Error,
          "non-finite values rejected");
}

void TestDecoderRecovery() {
    using Controller = phonecast::core::DecoderRecoveryController;
    Controller recovery;
    const auto start = Controller::TimePoint{};

    Check(!recovery.ObserveWindow(30, 1), "decoded output keeps watchdog healthy");
    Check(!recovery.ObserveWindow(0, 0), "static input does not trip watchdog");
    Check(!recovery.ObserveWindow(30, 0), "first decode stall window is tolerated");
    Check(!recovery.ObserveWindow(30, 0), "second decode stall window is tolerated");
    Check(recovery.ObserveWindow(30, 0), "third active decode stall requests recovery");

    recovery.Begin(start);
    Check(recovery.Active() && !recovery.Ready(start), "recovery starts with a short reset delay");
    const auto first = start + std::chrono::milliseconds(100);
    Check(recovery.Ready(first), "first decoder restart becomes ready");
    recovery.RecordAttempt(false, first);
    Check(recovery.Active() && recovery.Attempts() == 1,
          "failed restart schedules another bounded attempt");
    Check(!recovery.Ready(first + std::chrono::milliseconds(249)),
          "restart backoff is enforced");
    const auto second = first + std::chrono::milliseconds(250);
    Check(recovery.Ready(second), "second restart becomes ready after backoff");
    recovery.RecordAttempt(false, second);
    const auto third = second + std::chrono::milliseconds(500);
    Check(recovery.Ready(third), "third restart uses increasing backoff");
    recovery.RecordAttempt(false, third);
    Check(!recovery.Active() && recovery.Exhausted() &&
              recovery.Attempts() == Controller::MaximumAttempts(),
          "decoder restart attempts are bounded");

    recovery.Reset();
    recovery.Begin(start);
    recovery.RecordAttempt(true, first);
    Check(!recovery.Active() && !recovery.Exhausted(),
          "successful decoder restart clears recovery state");
}

void TestGeneratedFrames() {
    phonecast::core::GeneratedVideoSource source(64, 64);
    phonecast::core::VideoFrame first;
    phonecast::core::VideoFrame second;
    std::string error;
    Check(source.NextFrame(first, error) && first.IsValid(), "first generated frame is valid");
    Check(source.NextFrame(second, error) && second.IsValid(), "second generated frame is valid");
    Check(first.sequence == 0 && second.sequence == 1, "frame sequence increments");
    Check(first.pixels != second.pixels, "animation changes frame pixels");
}

void TestStreamProtocol() {
    using namespace phonecast::core::protocol;
    Message outgoing;
    outgoing.type = MessageType::VideoFrame;
    outgoing.flags = MessageFlags::KeyFrame;
    outgoing.sequence = 42;
    outgoing.timestampMicros = 123456;
    outgoing.width = 1080;
    outgoing.height = 1920;
    outgoing.payload = {0, 0, 0, 1, 0x65};
    const auto bytes = Serialize(outgoing);
    Check(bytes.size() == kHeaderSize + outgoing.payload.size(), "protocol message serializes");

    Message parsed;
    std::uint32_t payloadSize = 0;
    std::string error;
    Check(ParseHeader(bytes.data(), kHeaderSize, parsed, payloadSize, error),
          "protocol header parses");
    Check(parsed.type == MessageType::VideoFrame && parsed.sequence == 42,
          "protocol identity fields round trip");
    Check(parsed.flags == MessageFlags::KeyFrame && parsed.timestampMicros == 123456,
          "protocol timing fields round trip");
    Check(parsed.width == 1080 && parsed.height == 1920 && payloadSize == 5,
          "protocol dimensions and payload length round trip");

    auto corrupt = bytes;
    corrupt[0] = 'X';
    Check(!ParseHeader(corrupt.data(), kHeaderSize, parsed, payloadSize, error),
          "invalid protocol magic is rejected");
    corrupt = bytes;
    corrupt[8] = 0x7f;
    Check(!ParseHeader(corrupt.data(), kHeaderSize, parsed, payloadSize, error),
          "oversized payload is rejected");
    Message keyFrameRequest;
    keyFrameRequest.type = MessageType::RequestKeyFrame;
    const auto requestBytes = Serialize(keyFrameRequest);
    Check(ParseHeader(requestBytes.data(), kHeaderSize, parsed, payloadSize, error) &&
              parsed.type == MessageType::RequestKeyFrame && payloadSize == 0,
          "key-frame recovery request round trips");
    Check(IsValidPairCode("123456") && !IsValidPairCode("12345x"),
          "pair codes require six digits");

    phonecast::core::PointerEvent pointer;
    pointer.type = phonecast::core::PointerEvent::Type::Scroll;
    pointer.normalizedX = 0.25F;
    pointer.normalizedY = 0.75F;
    pointer.scrollDelta = -0.5F;
    const auto pointerBytes = SerializePointerEvent(pointer);
    phonecast::core::PointerEvent parsedPointer;
    Check(ParsePointerEvent(pointerBytes.data(), pointerBytes.size(), parsedPointer, error) &&
              parsedPointer.type == phonecast::core::PointerEvent::Type::Scroll &&
              parsedPointer.normalizedX == 0.25F && parsedPointer.normalizedY == 0.75F &&
              parsedPointer.scrollDelta == -0.5F,
          "normalized remote input round trips");
    auto invalidPointer = pointerBytes;
    invalidPointer[4] = 0x7f;
    invalidPointer[5] = 0x80;
    invalidPointer[6] = 0x00;
    invalidPointer[7] = 0x00;
    Check(!ParsePointerEvent(invalidPointer.data(), invalidPointer.size(), parsedPointer, error),
          "non-finite remote input is rejected");

    NotificationEvent notification;
    notification.applicationName = "Messages";
    notification.title = "Are you coming?";
    notification.body = "Dinner is ready";
    notification.packageName = "com.example.messages";
    notification.postedAtMillis = 123456789;
    notification.actionToken = 987654321;
    notification.contentRedacted = false;
    const auto notificationBytes = SerializeNotification(notification);
    NotificationEvent parsedNotification;
    Check(ParseNotification(notificationBytes.data(), notificationBytes.size(),
                            parsedNotification, error) &&
              parsedNotification.applicationName == notification.applicationName &&
              parsedNotification.title == notification.title &&
              parsedNotification.body == notification.body &&
              parsedNotification.packageName == notification.packageName &&
              parsedNotification.postedAtMillis == notification.postedAtMillis &&
              parsedNotification.actionToken == notification.actionToken &&
              !parsedNotification.contentRedacted,
          "privacy-filtered notification payload round trips");
    std::vector<std::uint8_t> legacyNotification(
        notificationBytes.begin(), notificationBytes.begin() + 18);
    legacyNotification[0] = 1;
    legacyNotification.insert(legacyNotification.end(),
                              notificationBytes.begin() + 26, notificationBytes.end());
    Check(ParseNotification(legacyNotification.data(), legacyNotification.size(),
                            parsedNotification, error) &&
              parsedNotification.actionToken == 0,
          "legacy notification payloads remain readable without an action");
    Message notificationOpen;
    notificationOpen.type = MessageType::NotificationOpen;
    notificationOpen.sequence = notification.actionToken;
    const auto notificationOpenBytes = Serialize(notificationOpen);
    Check(ParseHeader(notificationOpenBytes.data(), kHeaderSize, parsed, payloadSize, error) &&
              parsed.type == MessageType::NotificationOpen &&
              parsed.sequence == notification.actionToken && payloadSize == 0,
          "notification open action token round trips in the common header");

    auto malformedNotification = notificationBytes;
    malformedNotification[2] = 0x7f;
    Check(!ParseNotification(malformedNotification.data(), malformedNotification.size(),
                             parsedNotification, error),
          "malformed notification lengths are rejected");

    RemoteControlStatus remoteStatus{true, false};
    const auto remoteStatusBytes = SerializeRemoteControlStatus(remoteStatus);
    RemoteControlStatus parsedRemoteStatus;
    Check(ParseRemoteControlStatus(remoteStatusBytes.data(), remoteStatusBytes.size(),
                                   parsedRemoteStatus, error) &&
              parsedRemoteStatus.appEnabled &&
              !parsedRemoteStatus.accessibilityEnabled &&
              !parsedRemoteStatus.Ready(),
          "remote-control consent gates round trip independently");
    Check(!ParseRemoteControlStatus(remoteStatusBytes.data(), 1, parsedRemoteStatus, error),
          "malformed remote-control gate status is rejected");
}

void TestOverlayInteraction() {
    Check(phonecast::vr::AdjustPanelDepth(1.0F, 1.0F, 0.04F) > 1.0F &&
          phonecast::vr::AdjustPanelDepth(1.0F, -1.0F, 0.04F) < 1.0F &&
          phonecast::vr::AdjustPanelDepth(1.0F, 0.1F, 0.04F) == 1.0F &&
          phonecast::vr::AdjustPanelDepth(3.0F, 1.0F, 0.04F) == 3.0F &&
          phonecast::vr::AdjustPanelDepth(0.20F, -1.0F, 0.04F) == 0.20F,
          "move-handle stick depth has correct direction, deadzone, and bounds");
    phonecast::vr::OverlayInteractionController interaction;
    interaction.SetSurfaceSize(1000, 2000);
    const auto down = interaction.PointerDown(250.0F, 500.0F);
    const auto move = interaction.PointerMove(750.0F, 1500.0F);
    const auto back = interaction.Back();
    Check(down.type == phonecast::core::PointerEvent::Type::Down &&
              down.normalizedX == 0.25F && down.normalizedY == 0.75F,
          "OpenVR coordinates map to normalized top-left phone coordinates");
    Check(move.sequence == down.sequence + 1 && move.normalizedX == 0.75F &&
              move.normalizedY == 0.25F,
          "pointer motion retains ordering and coordinate orientation");
    Check(back.type == phonecast::core::PointerEvent::Type::Back &&
              back.sequence == move.sequence + 1,
          "Android Back is represented independently of renderer APIs");

    interaction.SetSurfaceSize(1000, 2000, 100);
    const auto top = interaction.PointerDown(250.0F, 2100.0F);
    const auto bottom = interaction.PointerUp(250.0F, 100.0F);
    Check(interaction.IsGrabHandle(50.0F) && !interaction.IsGrabHandle(100.0F),
          "the bottom inset is reserved for the overlay grab handle");
    Check(interaction.IsBackButton(50.0F, 50.0F) &&
              !interaction.IsBackButton(100.0F, 50.0F) &&
              !interaction.IsBackButton(50.0F, 100.0F),
          "the lower-left handle corner is reserved for Android Back");
    Check(interaction.IsCloseButton(850.0F, 50.0F) &&
              !interaction.IsCloseButton(50.0F, 50.0F) &&
              !interaction.IsCloseButton(850.0F, 100.0F),
          "Close is separated from Back and placed beside the resize corner");
    Check(interaction.IsResizeButton(950.0F, 50.0F) &&
              !interaction.IsResizeButton(899.0F, 50.0F) &&
              !interaction.IsResizeButton(950.0F, 100.0F),
          "the lower-right handle corner is reserved for resizing the phone");
    Check(top.normalizedY == 0.0F && bottom.normalizedY == 1.0F,
          "the grab handle inset does not offset phone touch coordinates");
}

void TestOverlayControls() {
    phonecast::vr::OverlaySettings initial;
    initial.widthMeters = 0.65F;
    initial.distanceMeters = 1.0F;
    phonecast::vr::OverlayController controls(initial);
    controls.Apply(phonecast::vr::OverlayAction::MoveRight);
    controls.Apply(phonecast::vr::OverlayAction::MoveUp);
    controls.Apply(phonecast::vr::OverlayAction::ScaleUp);
    Check(controls.Settings().offsetXMeters > 0.0F &&
              controls.Settings().offsetYMeters > 0.0F,
          "overlay controls move the transform");
    Check(controls.Settings().widthMeters > initial.widthMeters,
          "overlay controls change scale");
    controls.Apply(phonecast::vr::OverlayAction::ToggleVisibility);
    Check(!controls.Visible(), "overlay visibility toggles");
    controls.Apply(phonecast::vr::OverlayAction::Reset);
    Check(controls.Visible() && controls.Settings().offsetXMeters == 0.0F &&
              controls.Settings().widthMeters == initial.widthMeters,
          "overlay reset restores initial state");
    for (int count = 0; count < 100; ++count) {
        controls.Apply(phonecast::vr::OverlayAction::OpacityDown);
        controls.Apply(phonecast::vr::OverlayAction::DistanceNearer);
    }
    Check(controls.Settings().alpha == 0.0F && controls.Settings().distanceMeters == 0.2F,
          "overlay controls clamp safe ranges");

    controls.Apply(phonecast::vr::OverlayAction::WorldLocked);
    Check(controls.Settings().placementMode == phonecast::vr::PlacementMode::WorldLocked &&
              !controls.Settings().worldTransformValid,
          "world mode requests a fresh anchor");
    auto anchored = controls.Settings();
    anchored.worldTransformValid = true;
    controls.ReplaceSettings(anchored);
    const float originalWorldX = controls.Settings().worldTransform[3];
    controls.Apply(phonecast::vr::OverlayAction::MoveRight);
    Check(controls.Settings().worldTransform[3] > originalWorldX,
          "world controls move the absolute anchor");
    controls.Apply(phonecast::vr::OverlayAction::LeftControllerLocked);
    Check(controls.Settings().placementMode == phonecast::vr::PlacementMode::LeftControllerLocked,
          "left controller mode is selectable");
    controls.Apply(phonecast::vr::OverlayAction::MoveRight);
    controls.Apply(phonecast::vr::OverlayAction::MoveUp);
    controls.Apply(phonecast::vr::OverlayAction::DistanceFarther);
    controls.Apply(phonecast::vr::OverlayAction::ScaleDown);
    controls.Apply(phonecast::vr::OverlayAction::ControllerTiltUp);
    controls.Apply(phonecast::vr::OverlayAction::ControllerYawRight);
    controls.Apply(phonecast::vr::OverlayAction::ControllerOrientationNext);
    Check(controls.Settings().leftController.lateralMeters > 0.0F &&
              controls.Settings().leftController.heightMeters > 0.10F &&
              controls.Settings().leftController.distanceMeters > 0.18F &&
              controls.Settings().leftController.scale < 1.0F &&
              controls.Settings().leftController.tiltDegrees > 0.0F &&
              controls.Settings().leftController.yawDegrees > 0.0F &&
              controls.Settings().leftController.orientation ==
                  phonecast::vr::ControllerOrientation::WorldUpright,
          "active hand calibration is independently adjustable");
    Check(controls.Settings().rightController.lateralMeters == 0.0F,
          "inactive hand calibration is unchanged");
    controls.Apply(phonecast::vr::OverlayAction::ResetControllerCalibration);
    Check(controls.Settings().leftController.lateralMeters == 0.0F &&
              controls.Settings().leftController.scale == 1.0F,
          "active hand calibration resets");
}

void TestGlanceMode() {
    phonecast::vr::OverlaySettings base;
    base.placementMode = phonecast::vr::PlacementMode::HeadLocked;
    base.widthMeters = 0.8F;
    phonecast::vr::GlanceController glance;
    Check(glance.State() == phonecast::vr::GlanceState::Hidden && !glance.Visible(),
          "glance mode starts hidden");
    glance.SetHand(phonecast::vr::GlanceHand::Right);
    Check(glance.Hand() == phonecast::vr::GlanceHand::Right,
          "keyboard fallback can select the glance hand");
    glance.Cycle(glance.Hand());
    auto presented = glance.PresentationSettings(base);
    Check(glance.State() == phonecast::vr::GlanceState::Glance && glance.Visible() &&
              presented.placementMode == phonecast::vr::PlacementMode::RightControllerLocked &&
              presented.widthMeters < base.widthMeters,
          "glance preview uses the initiating hand and reduced size");
    glance.Cycle(phonecast::vr::GlanceHand::Right);
    presented = glance.PresentationSettings(base);
    Check(glance.State() == phonecast::vr::GlanceState::Expanded &&
              presented.placementMode == base.placementMode &&
              presented.widthMeters == base.widthMeters,
          "expanded view restores normal placement and size");
    glance.Cycle(phonecast::vr::GlanceHand::Right);
    Check(glance.State() == phonecast::vr::GlanceState::Pinned && glance.Visible(),
          "expanded view can be pinned");
    glance.Cycle(phonecast::vr::GlanceHand::Right);
    Check(glance.State() == phonecast::vr::GlanceState::Hidden && !glance.Visible(),
          "pinned view cycles back to hidden");
    glance.ToggleExpanded();
    Check(glance.State() == phonecast::vr::GlanceState::Expanded,
          "quick toggle opens the expanded view");
    glance.ToggleExpanded();
    Check(!glance.Visible(), "quick toggle hides a visible view");
    glance.ShowGlance(phonecast::vr::GlanceHand::Left);
    presented = glance.PresentationSettings(base);
    Check(glance.State() == phonecast::vr::GlanceState::Glance &&
              presented.placementMode == phonecast::vr::PlacementMode::LeftControllerLocked,
          "radial menu can request a left-hand glance preview");
    glance.ShowPinned();
    Check(glance.State() == phonecast::vr::GlanceState::Pinned && glance.Visible(),
          "radial menu can pin the normal presentation");
}

void TestOverlaySettingsPersistence() {
    const auto path = std::filesystem::temp_directory_path() / "phonecast-overlay-settings-test.ini";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    phonecast::vr::OverlaySettingsStore store(path);
    phonecast::vr::OverlaySettings saved;
    saved.placementMode = phonecast::vr::PlacementMode::WorldLocked;
    saved.worldTransformValid = true;
    saved.worldTransform[3] = 2.25F;
    saved.widthMeters = 0.9F;
    saved.leftController.distanceMeters = 0.42F;
    saved.leftController.heightMeters = -0.12F;
    saved.leftController.lateralMeters = 0.08F;
    saved.leftController.tiltDegrees = 25.0F;
    saved.leftController.yawDegrees = -15.0F;
    saved.leftController.scale = 1.4F;
    saved.leftController.orientation = phonecast::vr::ControllerOrientation::ControllerRelative;
    saved.rightController.orientation = phonecast::vr::ControllerOrientation::Wrist;
    saved.glancePreviewScale = 0.7F;
    saved.radialLongPressMilliseconds = 850;
    saved.notificationWidthMeters = 0.75F;
    saved.notificationDistanceMeters = 0.65F;
    saved.notificationOffsetXMeters = -0.15F;
    saved.notificationOffsetYMeters = 0.05F;
    std::string error;
    Check(store.Save(saved, error), "overlay settings save");
    phonecast::vr::OverlaySettings loaded;
    bool found = false;
    Check(store.Load(loaded, found, error) && found, "overlay settings load");
    Check(loaded.placementMode == phonecast::vr::PlacementMode::WorldLocked &&
              loaded.worldTransformValid && loaded.worldTransform[3] == 2.25F &&
              loaded.widthMeters == 0.9F &&
              loaded.leftController.distanceMeters == 0.42F &&
              loaded.leftController.heightMeters == -0.12F &&
              loaded.leftController.lateralMeters == 0.08F &&
              loaded.leftController.tiltDegrees == 25.0F &&
              loaded.leftController.yawDegrees == -15.0F &&
              loaded.leftController.scale == 1.4F &&
              loaded.leftController.orientation == phonecast::vr::ControllerOrientation::ControllerRelative &&
              loaded.rightController.orientation == phonecast::vr::ControllerOrientation::Wrist &&
              loaded.glancePreviewScale == 0.7F &&
              loaded.radialLongPressMilliseconds == 850 &&
              loaded.notificationWidthMeters == 0.75F &&
              loaded.notificationDistanceMeters == 0.65F &&
              loaded.notificationOffsetXMeters == -0.15F &&
              loaded.notificationOffsetYMeters == 0.05F,
          "overlay settings round trip");
    std::filesystem::remove(path, ignored);
}

void TestSettingsMenu() {
    phonecast::vr::OverlaySettings initial;
    initial.widthMeters = 0.65F;
    phonecast::vr::SettingsMenuController menu;
    menu.Open(initial);
    Check(menu.IsOpen() && menu.View().title == "PHONECAST SETTINGS",
          "settings menu opens at its root");
    auto rendererUpdate = initial;
    rendererUpdate.widthMeters = 0.80F;
    rendererUpdate.worldTransformValid = true;
    rendererUpdate.worldTransform[3] = 1.25F;
    menu.MergeRendererUpdate(rendererUpdate);
    Check(menu.Draft().widthMeters == 0.80F && menu.Draft().worldTransformValid &&
              menu.Draft().worldTransform[3] == 1.25F &&
              menu.Original().widthMeters == initial.widthMeters &&
              !menu.Original().worldTransformValid,
          "renderer resize and anchors merge into the draft without changing cancel state");
    menu.Handle(phonecast::vr::SettingsMenuCommand::Activate);
    Check(menu.View().title == "APPEARANCE", "settings menu opens Appearance");
    Check(menu.Handle(phonecast::vr::SettingsMenuCommand::Increase) ==
              phonecast::vr::SettingsMenuResult::Updated &&
              menu.Draft().widthMeters > initial.widthMeters,
          "settings changes update the transactional draft");
    Check(menu.View().normalizedValues[0] >= 0.0F &&
              menu.Handle({phonecast::vr::SettingsMenuCommand::SetNormalized, 1.0F}) ==
                  phonecast::vr::SettingsMenuResult::Updated &&
              menu.Draft().widthMeters == 2.0F,
          "settings sliders expose and apply normalized values");
    menu.Handle(phonecast::vr::SettingsMenuCommand::Back);
    for (int index = 0; index < 9; ++index)
        menu.Handle(phonecast::vr::SettingsMenuCommand::NextItem);
    Check(menu.Handle(phonecast::vr::SettingsMenuCommand::Activate) ==
              phonecast::vr::SettingsMenuResult::Applied && !menu.IsOpen(),
          "settings can be applied explicitly");

    menu.Open(initial);
    menu.Handle(phonecast::vr::SettingsMenuCommand::NextItem);
    menu.Handle(phonecast::vr::SettingsMenuCommand::NextItem);
    menu.Handle(phonecast::vr::SettingsMenuCommand::NextItem);
    menu.Handle(phonecast::vr::SettingsMenuCommand::NextItem);
    menu.Handle(phonecast::vr::SettingsMenuCommand::Activate);
    Check(menu.View().title == "GLANCE AND CONTROLS", "Glance settings are reachable");
    menu.Handle(phonecast::vr::SettingsMenuCommand::Increase);
    menu.Handle(phonecast::vr::SettingsMenuCommand::NextItem);
    menu.Handle(phonecast::vr::SettingsMenuCommand::Increase);
    Check(menu.Draft().glancePreviewScale > initial.glancePreviewScale &&
              menu.Draft().radialLongPressMilliseconds > initial.radialLongPressMilliseconds,
          "Glance preview and long-press settings are editable");
    Check(menu.Handle(phonecast::vr::SettingsMenuCommand::Back) ==
              phonecast::vr::SettingsMenuResult::None,
          "Back returns from a settings page");
    for (int index = 0; index < 5; ++index)
        menu.Handle(phonecast::vr::SettingsMenuCommand::NextItem);
    menu.Handle(phonecast::vr::SettingsMenuCommand::Activate);
    Check(menu.View().title == "NOTIFICATIONS" &&
              menu.View().showNotificationPreview,
          "Notification placement settings request a live card preview");
    menu.Handle(phonecast::vr::SettingsMenuCommand::Increase);
    menu.Handle(phonecast::vr::SettingsMenuCommand::NextItem);
    menu.Handle(phonecast::vr::SettingsMenuCommand::Decrease);
    menu.Handle(phonecast::vr::SettingsMenuCommand::NextItem);
    menu.Handle(phonecast::vr::SettingsMenuCommand::Decrease);
    menu.Handle(phonecast::vr::SettingsMenuCommand::NextItem);
    menu.Handle(phonecast::vr::SettingsMenuCommand::Decrease);
    Check(menu.Draft().notificationWidthMeters > initial.notificationWidthMeters &&
              menu.Draft().notificationDistanceMeters < initial.notificationDistanceMeters &&
              menu.Draft().notificationOffsetXMeters < initial.notificationOffsetXMeters &&
              menu.Draft().notificationOffsetYMeters < initial.notificationOffsetYMeters,
          "notification size and head-relative placement are editable");
    Check(menu.Handle(phonecast::vr::SettingsMenuCommand::Back) ==
              phonecast::vr::SettingsMenuResult::None,
          "Back returns from notification settings");
    Check(menu.Handle(phonecast::vr::SettingsMenuCommand::Back) ==
              phonecast::vr::SettingsMenuResult::Cancelled && !menu.IsOpen() &&
              menu.Original().widthMeters == initial.widthMeters,
          "Back from the root cancels the transaction");
}

void TestWristMenuGesture() {
    using phonecast::vr::WristMenuGestureAction;
    phonecast::vr::WristMenuGesture gesture;
    phonecast::vr::WristMenuGestureObservation observation;

    observation.nowMilliseconds = 100;
    Check(gesture.Update(observation).action == WristMenuGestureAction::None,
          "neutral wrists arm the gesture without opening");
    observation.leftFacing = true;
    observation.nowMilliseconds = 200;
    Check(gesture.Update(observation).action == WristMenuGestureAction::None,
          "wrist gesture waits for its opening dwell");
    observation.nowMilliseconds = 1700;
    const auto halfway = gesture.Update(observation);
    Check(halfway.action == WristMenuGestureAction::None &&
              halfway.progress > 0.45F && halfway.progress < 0.55F,
          "wrist gesture reports visible hold progress");
    observation.nowMilliseconds = 3200;
    Check(gesture.Update(observation).action == WristMenuGestureAction::OpenLeft,
          "stable three-second left wrist flip opens the left menu");

    observation.menuVisible = true;
    observation.leftFacing = false;
    observation.nowMilliseconds = 1200;
    Check(gesture.Update(observation).action == WristMenuGestureAction::None,
          "open menu remains available after the wrist moves away");

    gesture.Reset();
    observation = {};
    observation.nowMilliseconds = 100;
    gesture.Update(observation);
    observation.rightFacing = true;
    observation.nowMilliseconds = 200;
    gesture.Update(observation);
    observation.nowMilliseconds = 3200;
    Check(gesture.Update(observation).action == WristMenuGestureAction::OpenRight,
          "stable three-second right wrist flip opens the right menu");
}

void TestReceiverLifecycle() {
    phonecast::core::GeneratedVideoSource source(64, 64);
    FakeOverlay overlay;
    FakeLogger logger;
    phonecast::apps::Receiver receiver(source, overlay, logger);
    std::string error;
    Check(receiver.Start({}, error), "receiver starts");
    Check(receiver.Tick(error), "receiver submits a frame");
    Check(overlay.frames == 1 && receiver.SubmittedFrames() == 1, "submission is counted");
    receiver.Stop();
    receiver.Stop();
    Check(overlay.stopped, "renderer stops safely");

    FakeOverlay quittingOverlay;
    phonecast::apps::Receiver quittingReceiver(source, quittingOverlay, logger);
    Check(quittingReceiver.Start({}, error), "second receiver starts");
    quittingOverlay.keepRunning = false;
    error = "stale";
    Check(!quittingReceiver.Tick(error) && error.empty(), "runtime shutdown is clean");
    quittingReceiver.Stop();
}

}  // namespace

int main() {
    TestConfig();
    TestDecoderRecovery();
    TestGeneratedFrames();
    TestStreamProtocol();
    TestOverlayInteraction();
    TestOverlayControls();
    TestGlanceMode();
    TestOverlaySettingsPersistence();
    TestSettingsMenu();
    TestWristMenuGesture();
    TestReceiverLifecycle();
    if (failures == 0) std::cout << "All PhoneCast tests passed.\n";
    return failures == 0 ? 0 : 1;
}
