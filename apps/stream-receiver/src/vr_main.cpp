#include "phonecast/core/logging/ConsoleLogger.h"
#include "phonecast/core/protocol/StreamProtocol.h"
#include "phonecast/platform/openvr/OpenVrOverlayRenderer.h"
#include "phonecast/platform/windows/MfH264Decoder.h"
#include "phonecast/platform/windows/TcpVideoServer.h"
#include "phonecast/vr/overlay/GlanceController.h"
#include "phonecast/vr/overlay/OverlayController.h"
#include "phonecast/vr/overlay/OverlaySettingsStore.h"
#include "phonecast/vr/overlay/SettingsMenuController.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using phonecast::core::protocol::Message;
using phonecast::core::protocol::MessageType;
using phonecast::vr::OverlayAction;

void PrintUsage() {
    std::cout << "Usage: phonecast-vr-stream-receiver --pair-code NNNNNN [--port N] [--settings PATH] [--diagnostic-visible]\n\n"
              << "Global controls (hold Ctrl+Alt):\n"
              << "  P          Quick show/hide expanded view\n"
              << "  G          Cycle Hidden/Glance/Expanded/Pinned\n"
              << "  S          Open in-headset settings\n"
              << "  + / -      Scale up/down\n"
              << "  Arrows     Move overlay\n"
              << "  PageUp/Down  Move nearer/farther\n"
              << "  ] / [      Increase/decrease opacity\n"
              << "  H / W      Head-locked / world-locked\n"
              << "  L / R      Left/right-controller-locked\n"
              << "  Home       Reset appearance and position\n"
              << "  End        Quit PhoneCast\n\n"
              << "Open the SteamVR dashboard and select PhoneCast for Show/Hide, Glance, Pin, Settings, placement, and Android Back.\n"
              << "Experimental wrist and thumbstick menu gestures are disabled.\n"
              << "With the SteamVR dashboard open, trigger taps/drags the phone; drag the horizontal handle below the phone to move the overlay.\n"
              << "Press left View or right Menu to select that hand and enter/leave calibration:\n"
              << "  Axis             Lateral / height\n"
              << "  Grip + axis      Yaw / tilt\n"
              << "  Trigger + axis   Scale / distance\n"
              << "  Pad click        Cycle orientation\n"
              << "  Grip + pad click Reset that hand's calibration\n";
}

std::filesystem::path DefaultSettingsPath() {
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData != nullptr && *localAppData != '\0')
        return std::filesystem::path(localAppData) / "PhoneCastVR" / "overlay-settings.ini";
    return "phonecast-overlay-settings.ini";
}

bool Pressed(int key) {
    static bool previous[256]{};
    const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
    const bool pressed = down && !previous[key];
    previous[key] = down;
    return pressed;
}

phonecast::core::VideoFrame MakeWaitingFrame() {
    phonecast::core::VideoFrame frame;
    frame.width = 320;
    frame.height = 180;
    frame.format = phonecast::core::PixelFormat::Rgba8;
    frame.pixels.resize(static_cast<std::size_t>(frame.width) * frame.height * 4U);
    for (std::uint32_t y = 0; y < frame.height; ++y) {
        for (std::uint32_t x = 0; x < frame.width; ++x) {
            const bool border = x < 6 || y < 6 || x >= frame.width - 6 || y >= frame.height - 6;
            const std::size_t offset = (static_cast<std::size_t>(y) * frame.width + x) * 4U;
            frame.pixels[offset] = border ? 35 : 12;
            frame.pixels[offset + 1] = border ? 125 : 18;
            frame.pixels[offset + 2] = border ? 220 : 28;
            frame.pixels[offset + 3] = 255;
        }
    }
    return frame;
}

phonecast::vr::OverlaySettings SettingsForStream(
        const phonecast::vr::OverlaySettings& settings,
        std::uint32_t width, std::uint32_t height) {
    auto adjusted = settings;
    // Treat the configured width as the portrait short edge. When the same
    // phone rotates, grow the landscape width so its diagonal/overall scale
    // stays constant instead of making the landscape panel look much smaller.
    if (height > 0 && width > height) {
        adjusted.widthMeters *= static_cast<float>(width) / static_cast<float>(height);
    }
    return adjusted;
}

bool PollControl(OverlayAction& action, bool& glanceCycle, bool& quickToggle,
                 bool& openSettings, bool& quit) {
    glanceCycle = false;
    quickToggle = false;
    openSettings = false;
    quit = false;
    const bool modified = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
                          (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    const struct Binding { int key; OverlayAction action; } bindings[] = {
        {VK_OEM_PLUS, OverlayAction::ScaleUp}, {VK_ADD, OverlayAction::ScaleUp},
        {VK_OEM_MINUS, OverlayAction::ScaleDown}, {VK_SUBTRACT, OverlayAction::ScaleDown},
        {VK_LEFT, OverlayAction::MoveLeft}, {VK_RIGHT, OverlayAction::MoveRight},
        {VK_UP, OverlayAction::MoveUp}, {VK_DOWN, OverlayAction::MoveDown},
        {VK_PRIOR, OverlayAction::DistanceNearer}, {VK_NEXT, OverlayAction::DistanceFarther},
        {VK_OEM_6, OverlayAction::OpacityUp}, {VK_OEM_4, OverlayAction::OpacityDown},
        {'H', OverlayAction::HeadLocked}, {'W', OverlayAction::WorldLocked},
        {'L', OverlayAction::LeftControllerLocked}, {'R', OverlayAction::RightControllerLocked},
        {VK_HOME, OverlayAction::Reset},
    };
    bool found = false;
    for (const auto& binding : bindings) {
        if (Pressed(binding.key) && modified && !found) {
            action = binding.action;
            found = true;
        }
    }
    if (Pressed('G') && modified) glanceCycle = true;
    if (Pressed('P') && modified) quickToggle = true;
    if (Pressed('S') && modified) openSettings = true;
    if (Pressed(VK_END) && modified) quit = true;
    return found;
}
}  // namespace

int main(int argc, char** argv) {
    std::string pairCode;
    std::uint16_t port = 49321;
    bool diagnosticVisible = false;
    std::filesystem::path settingsPath = DefaultSettingsPath();
    for (int index = 1; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--help" || option == "-h") {
            PrintUsage();
            return EXIT_SUCCESS;
        }
        if (option == "--diagnostic-visible") {
            diagnosticVisible = true;
            continue;
        }
        if (index + 1 >= argc) {
            PrintUsage();
            return EXIT_FAILURE;
        }
        const std::string value = argv[++index];
        if (option == "--pair-code") pairCode = value;
        else if (option == "--settings") settingsPath = value;
        else if (option == "--port") {
            try {
                const unsigned long parsed = std::stoul(value);
                if (parsed == 0 || parsed > 65535) throw std::out_of_range("port");
                port = static_cast<std::uint16_t>(parsed);
            } catch (...) {
                std::cerr << "Invalid TCP port.\n";
                return EXIT_FAILURE;
            }
        } else {
            std::cerr << "Unknown option: " << option << '\n';
            return EXIT_FAILURE;
        }
    }
    if (!phonecast::core::protocol::IsValidPairCode(pairCode)) {
        std::cerr << "A six-digit --pair-code is required.\n";
        PrintUsage();
        return EXIT_FAILURE;
    }

    phonecast::core::ConsoleLogger logger;
    phonecast::platform::openvr::OpenVrOverlayRenderer renderer(logger, argv[0]);
    phonecast::vr::OverlaySettings initialSettings;
    phonecast::vr::OverlaySettingsStore settingsStore(settingsPath);
    bool settingsFound = false;
    std::string error;
    if (diagnosticVisible) {
        std::cout << "[diagnostic] Visible head-locked startup with default appearance; saved settings are not read or written.\n";
    } else if (!settingsStore.Load(initialSettings, settingsFound, error)) {
        std::cerr << "Warning: " << error << " Defaults will be used.\n";
        initialSettings = {};
    } else if (settingsFound) {
        std::cout << "Loaded overlay placement from " << settingsPath.string() << ".\n";
    }
    phonecast::vr::OverlayController controls;
    controls.ReplaceSettings(initialSettings);
    phonecast::vr::GlanceController glance(initialSettings.glancePreviewScale);
    if (diagnosticVisible) glance.ShowPinned();
    phonecast::vr::SettingsMenuController settingsMenu;
    if (!renderer.Start(controls.Settings(), error)) {
        const auto mode = controls.Settings().placementMode;
        const bool controllerMode =
            mode == phonecast::vr::PlacementMode::LeftControllerLocked ||
            mode == phonecast::vr::PlacementMode::RightControllerLocked;
        if (!controllerMode) {
            std::cerr << error << '\n';
            return EXIT_FAILURE;
        }
        std::cerr << "Warning: " << error << " Falling back to head-locked placement.\n";
        controls.Apply(OverlayAction::HeadLocked);
        if (!renderer.Start(controls.Settings(), error)) {
            std::cerr << error << '\n';
            return EXIT_FAILURE;
        }
    }
    if (!renderer.SetVisible(glance.Visible(), error)) {
        std::cerr << error << '\n';
        renderer.Stop();
        return EXIT_FAILURE;
    }
    const auto waitingFrame = MakeWaitingFrame();
    if (!renderer.SubmitFrame(waitingFrame, error)) {
        std::cerr << error << '\n';
        renderer.Stop();
        return EXIT_FAILURE;
    }
    phonecast::platform::windows::TcpVideoServer server(pairCode, port);
    if (!server.Start(error)) {
        std::cerr << error << '\n';
        renderer.Stop();
        return EXIT_FAILURE;
    }
    PrintUsage();
    std::cout << "\nPhoneCast VR receiver listening on port " << port
              << ". Pairing code: " << pairCode << '\n';

    phonecast::platform::windows::MfH264Decoder decoder;
    bool decoderStarted = false;
    bool running = true;
    std::vector<std::uint8_t> codecConfig;
    std::uint64_t windowDecodedFrames = 0;
    std::uint64_t windowRenderedFrames = 0;
    double windowDecodeMillis = 0.0;
    double windowRenderMillis = 0.0;
    double windowQueueMillis = 0.0;
    double maximumQueueMillis = 0.0;
    std::uint64_t windowMessages = 0;
    phonecast::platform::windows::VideoServerStats previousServerStats{};
    auto lastStats = std::chrono::steady_clock::now();
    auto connectionStarted = lastStats;
    bool wasConnected = false;
    bool configReported = false;
    bool keyFrameReported = false;
    bool firstFrameReported = false;
    std::uint32_t streamWidth = 0;
    std::uint32_t streamHeight = 0;
    bool notificationShowing = false;
    std::chrono::steady_clock::time_point notificationDeadline{};
    bool previousRemoteStatusKnown = false;
    phonecast::core::protocol::RemoteControlStatus previousRemoteStatus{};

    while (running && renderer.PumpEvents()) {
        OverlayAction action{};
        bool glanceCycle = false;
        bool quickToggle = false;
        bool keyboardSettings = false;
        bool quit = false;
        const bool settingsAction = PollControl(
            action, glanceCycle, quickToggle, keyboardSettings, quit);
        phonecast::vr::GlanceInput glanceInput{};
        const bool controllerCycle = renderer.TakeGlanceInput(glanceInput);
        phonecast::vr::RadialMenuSelection radialSelection{};
        const bool radialSelected = renderer.TakeRadialMenuSelection(radialSelection);
        if (!settingsMenu.IsOpen() &&
            (settingsAction || glanceCycle || quickToggle || keyboardSettings ||
             controllerCycle || radialSelected)) {
            const auto previousSettings = controls.Settings();
            bool saveSettings = settingsAction;
            bool openSettingsRequested = keyboardSettings;
            if (keyboardSettings) settingsMenu.Open(controls.Settings());
            if (settingsAction) {
                controls.Apply(action);
                if (action == OverlayAction::LeftControllerLocked)
                    glance.SetHand(phonecast::vr::GlanceHand::Left);
                else if (action == OverlayAction::RightControllerLocked)
                    glance.SetHand(phonecast::vr::GlanceHand::Right);
            }
            if (glanceCycle) glance.Cycle(glance.Hand());
            if (quickToggle) glance.ToggleExpanded();
            if (controllerCycle) {
                logger.Log(phonecast::core::LogLevel::Info, "input", "Controller glance click received.");
                glance.Cycle(glanceInput == phonecast::vr::GlanceInput::LeftController
                    ? phonecast::vr::GlanceHand::Left : phonecast::vr::GlanceHand::Right);
            }
            if (radialSelected) {
                const auto hand = radialSelection.hand == phonecast::vr::GlanceInput::LeftController
                    ? phonecast::vr::GlanceHand::Left : phonecast::vr::GlanceHand::Right;
                glance.SetHand(hand);
                switch (radialSelection.action) {
                    case phonecast::vr::RadialMenuAction::ToggleVisible:
                        glance.ToggleExpanded();
                        break;
                    case phonecast::vr::RadialMenuAction::ShowGlance:
                        glance.ShowGlance(hand);
                        break;
                    case phonecast::vr::RadialMenuAction::ShowPinned:
                        glance.ShowPinned();
                        break;
                    case phonecast::vr::RadialMenuAction::HeadLocked:
                        controls.Apply(OverlayAction::HeadLocked);
                        glance.ShowPinned();
                        saveSettings = true;
                        break;
                    case phonecast::vr::RadialMenuAction::WorldLocked:
                        controls.Apply(OverlayAction::WorldLocked);
                        glance.ShowPinned();
                        saveSettings = true;
                        break;
                    case phonecast::vr::RadialMenuAction::LeftControllerLocked:
                        controls.Apply(OverlayAction::LeftControllerLocked);
                        glance.SetHand(phonecast::vr::GlanceHand::Left);
                        glance.ShowPinned();
                        saveSettings = true;
                        break;
                    case phonecast::vr::RadialMenuAction::RightControllerLocked:
                        controls.Apply(OverlayAction::RightControllerLocked);
                        glance.SetHand(phonecast::vr::GlanceHand::Right);
                        glance.ShowPinned();
                        saveSettings = true;
                        break;
                    case phonecast::vr::RadialMenuAction::OpenSettings:
                        settingsMenu.Open(controls.Settings());
                        openSettingsRequested = true;
                        break;
                }
            }
            const auto displayedSettings = SettingsForStream(
                glance.PresentationSettings(controls.Settings()), streamWidth, streamHeight);
            if (!renderer.ApplySettings(displayedSettings, error) ||
                !renderer.SetVisible(glance.Visible() && controls.Visible(), error)) {
                std::cerr << error << '\n';
                controls.ReplaceSettings(previousSettings);
                glance.Dismiss();
                std::string ignored;
                renderer.ApplySettings(SettingsForStream(previousSettings, streamWidth, streamHeight), ignored);
                renderer.SetVisible(false, ignored);
            } else {
                if (saveSettings && !diagnosticVisible && !settingsStore.Save(controls.Settings(), error))
                    std::cerr << "Warning: " << error << '\n';
                if (openSettingsRequested &&
                    !renderer.ShowSettingsMenu(settingsMenu.View(), error)) {
                    std::cerr << error << '\n';
                    settingsMenu.Close();
                }
            }
        }
        if (quit) break;

        phonecast::vr::SettingsMenuInput settingsInput{};
        if (settingsMenu.IsOpen() && renderer.TakeSettingsMenuInput(settingsInput)) {
            const auto result = settingsMenu.Handle(settingsInput);
            if (result == phonecast::vr::SettingsMenuResult::Updated ||
                result == phonecast::vr::SettingsMenuResult::Applied) {
                controls.ReplaceSettings(settingsMenu.Draft());
                glance.SetPreviewScale(settingsMenu.Draft().glancePreviewScale);
                const auto displayed = SettingsForStream(
                    glance.PresentationSettings(controls.Settings()), streamWidth, streamHeight);
                if (!renderer.ApplySettings(displayed, error))
                    std::cerr << error << '\n';
            } else if (result == phonecast::vr::SettingsMenuResult::Cancelled) {
                controls.ReplaceSettings(settingsMenu.Original());
                glance.SetPreviewScale(settingsMenu.Original().glancePreviewScale);
                const auto displayed = SettingsForStream(
                    glance.PresentationSettings(controls.Settings()), streamWidth, streamHeight);
                if (!renderer.ApplySettings(displayed, error))
                    std::cerr << error << '\n';
            }

            if (result == phonecast::vr::SettingsMenuResult::Applied) {
                if (!diagnosticVisible && !settingsStore.Save(controls.Settings(), error))
                    std::cerr << "Warning: " << error << '\n';
                if (!renderer.HideSettingsMenu(error)) std::cerr << error << '\n';
            } else if (result == phonecast::vr::SettingsMenuResult::Cancelled) {
                if (!renderer.HideSettingsMenu(error)) std::cerr << error << '\n';
            } else if (!renderer.ShowSettingsMenu(settingsMenu.View(), error)) {
                std::cerr << error << '\n';
                settingsMenu.Close();
            }
        }

        std::uint64_t notificationActionToken = 0;
        if (renderer.TakeNotificationOpenRequest(notificationActionToken)) {
            glance.ShowExpanded();
            const auto displayed = SettingsForStream(
                glance.PresentationSettings(controls.Settings()), streamWidth, streamHeight);
            if (!renderer.ApplySettings(displayed, error) ||
                !renderer.SetVisible(controls.Visible(), error)) {
                std::cerr << error << '\n';
            }
            if (notificationActionToken == 0) {
                std::cerr << "[notification] The source notification supplied no Android open action; "
                             "showing the phone only.\n";
            } else if (!server.SendNotificationOpen(notificationActionToken, error)) {
                std::cerr << "[notification] " << error << '\n';
            } else {
                std::cout << "[notification] Sent the Android notification open action.\n";
            }
            notificationShowing = false;
        }

        if (renderer.TakeHideRequest()) {
            glance.Dismiss();
            if (!renderer.SetVisible(false, error)) std::cerr << error << '\n';
        }

        phonecast::core::PointerEvent pointerEvent;
        while (renderer.TakePointerEvent(pointerEvent)) {
            if (!server.Send(pointerEvent, error))
                std::cerr << "[input] " << error << '\n';
        }

        phonecast::vr::OverlaySettings vrUpdate;
        if (renderer.TakeSettingsUpdate(vrUpdate)) {
            if (settingsMenu.IsOpen()) {
                settingsMenu.MergeRendererUpdate(vrUpdate);
                controls.ReplaceSettings(settingsMenu.Draft());
            } else {
                auto merged = controls.Settings();
                merged.placementMode = vrUpdate.placementMode;
                merged.leftController = vrUpdate.leftController;
                merged.rightController = vrUpdate.rightController;
                merged.worldTransform = vrUpdate.worldTransform;
                merged.worldTransformValid = vrUpdate.worldTransformValid;
                controls.ReplaceSettings(merged);
                if (!diagnosticVisible && !settingsStore.Save(controls.Settings(), error))
                    std::cerr << "Warning: " << error << '\n';
            }
        }

        const bool connected = server.Connected();
        const bool remoteStatusKnown = connected && server.RemoteControlStatusKnown();
        const auto remoteStatus = server.RemoteControlStatus();
        if (remoteStatusKnown != previousRemoteStatusKnown ||
            remoteStatus.appEnabled != previousRemoteStatus.appEnabled ||
            remoteStatus.accessibilityEnabled != previousRemoteStatus.accessibilityEnabled) {
            renderer.SetRemoteControlStatus(remoteStatusKnown, remoteStatus.appEnabled,
                                            remoteStatus.accessibilityEnabled);
            if (remoteStatusKnown && !remoteStatus.Ready()) {
                std::cerr << "[input] Warning: remote control is unavailable; "
                          << (!remoteStatus.appEnabled && !remoteStatus.accessibilityEnabled
                                  ? "enable both the Android app consent and Accessibility service."
                              : !remoteStatus.appEnabled
                                  ? "enable Allow remote control while casting in the Android app."
                                  : "enable the PhoneCast Accessibility service on Android.")
                          << '\n';
            } else if (remoteStatusKnown) {
                std::cout << "[input] Both Android remote-control gates are enabled.\n";
            }
            previousRemoteStatusKnown = remoteStatusKnown;
            previousRemoteStatus = remoteStatus;
        }
        if (connected && !wasConnected) {
            connectionStarted = std::chrono::steady_clock::now();
            configReported = false;
            keyFrameReported = false;
            firstFrameReported = false;
        }
        wasConnected = connected;

        Message message;
        phonecast::core::VideoFrame latestFrame;
        bool haveFrame = false;
        std::chrono::microseconds queueAge{};
        while (server.Pop(message, &queueAge)) {
            const double queueMillis = queueAge.count() / 1000.0;
            windowQueueMillis += queueMillis;
            maximumQueueMillis = std::max(maximumQueueMillis, queueMillis);
            ++windowMessages;
            if (message.type == MessageType::Notification) {
                phonecast::core::protocol::NotificationEvent notification;
                if (!phonecast::core::protocol::ParseNotification(
                        message.payload.data(), message.payload.size(), notification, error)) {
                    std::cerr << "[notification] Rejected malformed card: " << error << '\n';
                } else if (!renderer.ShowNotification(notification, error)) {
                    std::cerr << "[notification] " << error << '\n';
                } else {
                    notificationShowing = true;
                    notificationDeadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(6);
                }
                continue;
            }
            if (message.type == MessageType::VideoConfig) {
                codecConfig = std::move(message.payload);
                streamWidth = message.width;
                streamHeight = message.height;
                const auto displayedSettings = SettingsForStream(
                    glance.PresentationSettings(controls.Settings()), streamWidth, streamHeight);
                if (!renderer.ApplySettings(displayedSettings, error)) {
                    std::cerr << error << '\n';
                    running = false;
                    break;
                }
                std::cout << "[stream] video-config dimensions=" << message.width
                          << 'x' << message.height
                          << " overlay-width-m=" << displayedSettings.widthMeters << '\n';
                if (!configReported) {
                    const double configMillis = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - connectionStarted).count();
                    std::cout << "[diagnostics] first-config-ms=" << configMillis
                              << " dimensions=" << message.width << 'x' << message.height << '\n';
                    configReported = true;
                }
                const auto decoderStartBegan = std::chrono::steady_clock::now();
                decoderStarted = decoder.Start(message.width, message.height, error);
                const double decoderStartMillis = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - decoderStartBegan).count();
                std::cout << "[diagnostics] decoder-start-ms=" << decoderStartMillis << '\n';
                if (!decoderStarted) std::cerr << error << '\n';
                continue;
            }
            if (!decoderStarted) continue;
            std::vector<std::uint8_t> accessUnit;
            if ((message.flags & phonecast::core::protocol::MessageFlags::KeyFrame) != 0) {
                if (!keyFrameReported) {
                    const double keyFrameMillis = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - connectionStarted).count();
                    std::cout << "[diagnostics] first-keyframe-ms=" << keyFrameMillis << '\n';
                    keyFrameReported = true;
                }
                accessUnit.reserve(codecConfig.size() + message.payload.size());
                accessUnit.insert(accessUnit.end(), codecConfig.begin(), codecConfig.end());
                accessUnit.insert(accessUnit.end(), message.payload.begin(), message.payload.end());
            } else {
                accessUnit = std::move(message.payload);
            }
            phonecast::core::VideoFrame frame;
            bool produced = false;
            const auto decodeStarted = std::chrono::steady_clock::now();
            if (!decoder.Submit(accessUnit, message.timestampMicros, frame, produced, error)) {
                std::cerr << error << '\n';
                decoderStarted = false;
            } else if (produced) {
                windowDecodeMillis += std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - decodeStarted).count();
                latestFrame = std::move(frame);
                haveFrame = true;
                ++windowDecodedFrames;
            }
        }
        if (haveFrame) {
            const auto renderStarted = std::chrono::steady_clock::now();
            if (!renderer.SubmitFrame(latestFrame, error)) {
                std::cerr << error << '\n';
                break;
            }
            windowRenderMillis += std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - renderStarted).count();
            ++windowRenderedFrames;
            if (!firstFrameReported) {
                const double startupMillis = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - connectionStarted).count();
                std::cout << "[diagnostics] first-submitted-frame-ms=" << startupMillis << '\n';
                firstFrameReported = true;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (notificationShowing && now >= notificationDeadline) {
            if (!renderer.HideNotification(error))
                std::cerr << "[notification] " << error << '\n';
            notificationShowing = false;
        }
        if (now - lastStats >= std::chrono::seconds(1)) {
            const double seconds = std::chrono::duration<double>(now - lastStats).count();
            const auto serverStats = server.Stats();
            const auto received = serverStats.receivedFrames - previousServerStats.receivedFrames;
            const auto bytes = serverStats.receivedBytes - previousServerStats.receivedBytes;
            const auto keyFrames = serverStats.keyFrames - previousServerStats.keyFrames;
            const auto dropped = serverStats.droppedFrames - previousServerStats.droppedFrames;
            const auto resyncs = serverStats.resyncRequests - previousServerStats.resyncRequests;
            std::cout << "[diagnostics] connected=" << (connected ? "yes" : "no")
                      << " rx-fps=" << received / seconds
                      << " decode-fps=" << windowDecodedFrames / seconds
                      << " render-fps=" << windowRenderedFrames / seconds
                      << " bitrate-mbps=" << bytes * 8.0 / seconds / 1'000'000.0
                      << " decode-ms=" << (windowDecodedFrames > 0 ? windowDecodeMillis / windowDecodedFrames : 0.0)
                      << " render-ms=" << (windowRenderedFrames > 0 ? windowRenderMillis / windowRenderedFrames : 0.0)
                      << " queue-ms=" << (windowMessages > 0 ? windowQueueMillis / windowMessages : 0.0)
                      << " queue-max-ms=" << maximumQueueMillis
                      << " queue-depth=" << serverStats.queueDepth
                      << " keyframes=" << keyFrames
                      << " dropped=" << dropped
                      << " resyncs=" << resyncs << '\n';
            previousServerStats = serverStats;
            windowDecodedFrames = 0;
            windowRenderedFrames = 0;
            windowDecodeMillis = 0.0;
            windowRenderMillis = 0.0;
            windowQueueMillis = 0.0;
            maximumQueueMillis = 0.0;
            windowMessages = 0;
            lastStats = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    server.Stop();
    decoder.Stop();
    renderer.Stop();
    return EXIT_SUCCESS;
}
