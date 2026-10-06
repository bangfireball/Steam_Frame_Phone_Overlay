#include "phonecast/core/logging/ConsoleLogger.h"
#include "phonecast/core/protocol/StreamProtocol.h"
#include "phonecast/core/streaming/DecoderRecoveryController.h"
#include "phonecast/core/streaming/DispatchBudget.h"
#include "phonecast/core/audio/AudioPlayback.h"
#include "phonecast/platform/openvr/OpenVrOverlayRenderer.h"
#include "phonecast/platform/network/TcpVideoServer.h"
#ifdef _WIN32
#include "phonecast/platform/windows/MfH264Decoder.h"
#include "phonecast/platform/windows/WasapiAudioOutput.h"
#include "phonecast/platform/windows/ProcessPerformanceSampler.h"
#else
#include "phonecast/platform/steamframe/ProcessPerformanceSampler.h"
#include "phonecast/platform/steamframe/StandaloneRuntime.h"
#include "phonecast/platform/steamframe/V4l2H264Decoder.h"
#include "phonecast/platform/steamframe/PulseAudioOutput.h"
#endif
#include "phonecast/vr/overlay/GlanceController.h"
#include "phonecast/vr/overlay/OverlayController.h"
#include "phonecast/vr/overlay/OverlaySettingsStore.h"
#include "phonecast/vr/overlay/SettingsMenuController.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <atomic>
#include <csignal>
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using phonecast::core::protocol::Message;
using phonecast::core::protocol::MessageType;
using phonecast::vr::OverlayAction;

#ifdef _WIN32
using PlatformDecoder = phonecast::platform::windows::MfH264Decoder;
using PlatformAudio = phonecast::platform::windows::WasapiAudioOutput;
using PlatformProcessSampler = phonecast::platform::windows::ProcessPerformanceSampler;
#else
using PlatformDecoder = phonecast::platform::steamframe::V4l2H264Decoder;
using PlatformAudio = phonecast::platform::steamframe::PulseAudioOutput;
using PlatformProcessSampler = phonecast::platform::steamframe::ProcessPerformanceSampler;
std::atomic_bool signalRequested{};
void HandleSignal(int) { signalRequested.store(true); }
#endif

void PrintUsage() {
    std::cout << "Usage: phonecast-vr-stream-receiver [--pair-code NNNNNN] [--pair-code-file PATH] [--port N] [--settings PATH] [--performance-log PATH] [--video-device PATH] [--focus-dashboard] [--diagnostic-visible]\n\n"
              << "Native Steam Frame launches create and persist a pairing code when none is supplied.\n"
              << "  --focus-dashboard      Open the PhoneCast dashboard after launch\n"
              << "  --pair-code-file PATH  Linux: override the persistent credential path\n\n"
              << "Performance measurement:\n"
              << "  --performance-log PATH  Write one-second receiver/process/VR samples as CSV\n"
              << "  --video-device PATH     Linux: select a stateful V4L2 H.264 decoder\n"
              << "  --audio-device ID       Select WASAPI endpoint/Pulse sink (default: VR hint/system sink)\n"
              << "                          Use 'default' to force the system default. Android opt-in is required.\n\n"
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
#ifdef _WIN32
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData != nullptr && *localAppData != '\0')
        return std::filesystem::path(localAppData) / "PhoneCastVR" / "overlay-settings.ini";
#else
    const char* configHome = std::getenv("XDG_CONFIG_HOME");
    if (configHome != nullptr && *configHome != '\0')
        return std::filesystem::path(configHome) / "phonecast-vr" / "overlay-settings.ini";
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0')
        return std::filesystem::path(home) / ".config" / "phonecast-vr" / "overlay-settings.ini";
#endif
    return "phonecast-overlay-settings.ini";
}

#ifndef _WIN32
std::filesystem::path DefaultPairCodePath() {
    const char* configHome = std::getenv("XDG_CONFIG_HOME");
    if (configHome != nullptr && *configHome != '\0')
        return std::filesystem::path(configHome) / "phonecast-vr" / "pairing-code";
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0')
        return std::filesystem::path(home) / ".config" / "phonecast-vr" / "pairing-code";
    return "phonecast-pairing-code";
}
#endif

#ifdef _WIN32
bool Pressed(int key) {
    static bool previous[256]{};
    const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
    const bool pressed = down && !previous[key];
    previous[key] = down;
    return pressed;
}
#endif

phonecast::core::VideoFrame MakeWaitingFrame() {
    phonecast::core::VideoFrame frame;
    // Match the sender's normal portrait aspect ratio before the first video
    // configuration arrives. A landscape placeholder made the fixed footer
    // consume a quarter of the surface and looked like a distorted phone.
    frame.width = 590;
    frame.height = 1280;
    frame.format = phonecast::core::PixelFormat::Rgba8;
    frame.pixels.resize(static_cast<std::size_t>(frame.width) * frame.height * 4U);
    for (std::uint32_t y = 0; y < frame.height; ++y) {
        for (std::uint32_t x = 0; x < frame.width; ++x) {
            const bool border = x < 3 || y < 3 || x >= frame.width - 3 || y >= frame.height - 3;
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
#ifdef _WIN32
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
#else
    (void)action;
    quit = signalRequested.load();
    return false;
#endif
}
}  // namespace

int main(int argc, char** argv) {
    std::string pairCode;
    std::uint16_t port = 49321;
    bool diagnosticVisible = false;
    bool focusDashboard = false;
    std::filesystem::path settingsPath = DefaultSettingsPath();
    std::filesystem::path performanceLogPath;
    std::filesystem::path pairCodePath;
    std::string videoDevice;
    std::string audioDevice;
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
        if (option == "--focus-dashboard") {
            focusDashboard = true;
            continue;
        }
        if (index + 1 >= argc) {
            PrintUsage();
            return EXIT_FAILURE;
        }
        const std::string value = argv[++index];
        if (option == "--pair-code") pairCode = value;
        else if (option == "--pair-code-file") pairCodePath = value;
        else if (option == "--settings") settingsPath = value;
        else if (option == "--performance-log") performanceLogPath = value;
        else if (option == "--video-device") videoDevice = value;
        else if (option == "--audio-device") audioDevice = value;
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
#ifdef _WIN32
    if (!pairCodePath.empty()) {
        std::cerr << "--pair-code-file is available only on the native Linux receiver.\n";
        return EXIT_FAILURE;
    }
    if (!videoDevice.empty()) {
        std::cerr << "--video-device is available only on the native Linux receiver.\n";
        return EXIT_FAILURE;
    }
    if (!phonecast::core::protocol::IsValidPairCode(pairCode)) {
        std::cerr << "A six-digit --pair-code is required on Windows.\n";
        PrintUsage();
        return EXIT_FAILURE;
    }
#else
    phonecast::platform::steamframe::StandaloneRuntime standaloneRuntime;
    std::string instanceError;
    const auto instanceResult = standaloneRuntime.Start(instanceError);
    if (instanceResult == phonecast::platform::steamframe::InstanceStartResult::ExistingSignaled) {
        std::cout << "PhoneCast is already running; requested its dashboard.\n";
        return EXIT_SUCCESS;
    }
    if (instanceResult == phonecast::platform::steamframe::InstanceStartResult::Error) {
        std::cerr << instanceError << '\n';
        return EXIT_FAILURE;
    }
    if (pairCode.empty()) {
        if (pairCodePath.empty()) pairCodePath = DefaultPairCodePath();
        std::string credentialError;
        if (!phonecast::platform::steamframe::LoadOrCreatePairCode(
                pairCodePath, pairCode, credentialError)) {
            std::cerr << credentialError << '\n';
            return EXIT_FAILURE;
        }
    }
    if (!phonecast::core::protocol::IsValidPairCode(pairCode)) {
        std::cerr << "The pairing code must contain exactly six digits.\n";
        return EXIT_FAILURE;
    }
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);
#endif

    std::ofstream performanceLog;
    if (!performanceLogPath.empty()) {
        std::error_code directoryError;
        const auto parent = performanceLogPath.parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent, directoryError);
        if (directoryError) {
            std::cerr << "Could not create performance-log directory: "
                      << directoryError.message() << '\n';
            return EXIT_FAILURE;
        }
        performanceLog.open(performanceLogPath, std::ios::out | std::ios::trunc);
        if (!performanceLog) {
            std::cerr << "Could not open performance log: "
                      << performanceLogPath.string() << '\n';
            return EXIT_FAILURE;
        }
        performanceLog << "elapsed_s,connected,width,height,rx_fps,decode_fps,render_fps,"
                          "bitrate_mbps,decode_ms,render_ms,queue_ms,queue_max_ms,queue_depth,"
                          "dropped,resyncs,process_cpu_percent,working_set_mb,private_mb,"
                          "vr_frame_index,vr_frame_presents,vr_mispresented,vr_dropped,"
                          "vr_reprojection_flags,vr_total_gpu_ms,vr_compositor_gpu_ms,"
                          "vr_compositor_cpu_ms,vr_client_interval_ms,first_config_ms,"
                          "first_keyframe_ms,decoder_start_ms,first_submitted_ms,"
                          "decoder_recovery_triggers,decoder_recovery_successes,"
                          "decoder_recovery_failed_attempts,audio_submitted,audio_dropped,audio_failures,"
                          "audio_queued,audio_output_latency_ms,audio_bitrate_mbps,"
                          "video_sync_queued,video_sync_deadline_releases,video_sync_overflow_releases,"
                          "video_sync_coalesced,video_sync_last_hold_ms,video_sync_skew_valid,video_sync_estimated_skew_ms,dispatch_max_messages,dispatch_max_ms,presentation_max_gap_ms,loop_max_ms,audio_sequence_gaps,audio_timestamp_gaps,audio_reanchors,audio_flushes,audio_opens,audio_stale_drops,audio_overflow_drops,audio_rejected_drops,audio_max_open_ms\n";
        std::cout << "Writing performance samples to " << performanceLogPath.string() << ".\n";
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
#ifndef _WIN32
    if (!diagnosticVisible && !settingsFound) {
        // The physically preferred Steam Frame default is intentionally smaller
        // than the PC-hosted panel. Existing persisted choices remain untouched.
        initialSettings.widthMeters = 0.20F;
    }
#endif
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
    renderer.SetPairingCode(pairCode);
    if (focusDashboard && !renderer.FocusDashboard(error))
        std::cerr << "Warning: " << error << '\n';
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
    phonecast::platform::network::TcpVideoServer server(pairCode, port, true);
    if (!server.Start(error)) {
        std::cerr << error << '\n';
        renderer.Stop();
        return EXIT_FAILURE;
    }
    PrintUsage();
    std::cout << "\nPhoneCast VR receiver listening on port " << port
              << ". The pairing credential is shown in the PhoneCast dashboard.\n";

#ifdef _WIN32
    PlatformDecoder decoder;
#else
    PlatformDecoder decoder(videoDevice);
#endif
    phonecast::core::audio::AudioPlayback audio([] { return std::make_unique<PlatformAudio>(); });
    phonecast::core::audio::AudioVideoQueue videoPlayout;
    std::uint64_t audioEpoch = 0;
    phonecast::core::audio::CaptureStatus captureAudioStatus = phonecast::core::audio::CaptureStatus::Off;
    std::uint64_t previousSessionGeneration = 0;
    std::uint64_t previousAudioBytes = 0;
    std::string previousAudioOutputStatus;
    std::string runtimeAudioDevice;
#ifdef _WIN32
    runtimeAudioDevice = renderer.DefaultAudioDeviceId();
#endif
    PlatformProcessSampler processSampler;
    processSampler.Sample();
    const auto sessionStarted = std::chrono::steady_clock::now();
    bool decoderStarted = false;
    bool running = true;
    phonecast::core::DecoderRecoveryController decoderRecovery;
    std::uint64_t decoderRecoveryTriggers = 0;
    std::uint64_t decoderRecoverySuccesses = 0;
    std::uint64_t decoderRecoveryFailedAttempts = 0;
    std::vector<std::uint8_t> codecConfig;
    std::uint64_t windowDecodedFrames = 0;
    std::uint64_t windowRenderedFrames = 0;
    double windowDecodeMillis = 0.0;
    double windowRenderMillis = 0.0;
    double windowQueueMillis = 0.0;
    double maximumQueueMillis = 0.0;
    std::uint64_t windowMessages = 0;
    std::size_t maximumDispatchMessages = 0;
    double maximumDispatchMillis = 0.0, maximumPresentationGapMillis = 0.0, maximumLoopMillis = 0.0;
    std::chrono::steady_clock::time_point previousPresentation{};
    phonecast::platform::network::VideoServerStats previousServerStats{};
    auto lastStats = std::chrono::steady_clock::now();
    auto connectionStarted = lastStats;
    bool wasConnected = false;
    bool configReported = false;
    bool keyFrameReported = false;
    bool firstFrameReported = false;
    double firstConfigMillis = -1.0;
    double firstKeyFrameMillis = -1.0;
    double decoderStartMillis = -1.0;
    double firstSubmittedMillis = -1.0;
    std::uint32_t streamWidth = 0;
    std::uint32_t streamHeight = 0;
    bool notificationShowing = false;
    std::chrono::steady_clock::time_point notificationDeadline{};
    bool previousRemoteStatusKnown = false;
    phonecast::core::protocol::RemoteControlStatus previousRemoteStatus{};

    const auto beginDecoderRecovery = [&](const std::string& reason) {
        decoder.Stop();
        decoderStarted = false;
        decoderRecovery.Begin(phonecast::core::DecoderRecoveryController::Clock::now());
        ++decoderRecoveryTriggers;
        std::cerr << "[decoder] " << reason
                  << " Restarting the decoder without closing the phone connection.\n";
        std::string resyncError;
        if (!server.RequestVideoResync(resyncError))
            std::cerr << "[decoder] Warning: " << resyncError << '\n';
    };

    while (running) {
        const auto loopStarted = std::chrono::steady_clock::now();
        if (!renderer.PumpEvents()) break;
#ifndef _WIN32
        if (standaloneRuntime.TakeDashboardFocusRequest() &&
            !renderer.FocusDashboard(error))
            std::cerr << "Warning: " << error << '\n';
#endif
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
        if (quit || renderer.TakeQuitRequest()) break;

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
        for (unsigned i = 0; i < 32 && renderer.TakePointerEvent(pointerEvent); ++i) {
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

        std::string selectedAudioDevice = audioDevice;
        if (audioDevice == "default") selectedAudioDevice.clear();
        else if (audioDevice.empty() && !controls.Settings().audioUseSystemDefault)
            selectedAudioDevice = runtimeAudioDevice;
        audio.SetControls(controls.Settings().audioMuted,controls.Settings().audioVolume,selectedAudioDevice);

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
        const auto generation = server.SessionGeneration();
        if (connected && (!wasConnected || generation != previousSessionGeneration)) {
            audio.Reset(); audioEpoch = 0; captureAudioStatus = phonecast::core::audio::CaptureStatus::Off;
            videoPlayout.Clear();
            previousSessionGeneration = generation;
            connectionStarted = std::chrono::steady_clock::now();
            configReported = false;
            keyFrameReported = false;
            firstFrameReported = false;
            firstConfigMillis = -1.0;
            firstKeyFrameMillis = -1.0;
            decoderStartMillis = -1.0;
            firstSubmittedMillis = -1.0;
            decoderRecovery.Reset();
        } else if (!connected && wasConnected) {
            audio.Reset(); audioEpoch = 0; captureAudioStatus = phonecast::core::audio::CaptureStatus::Off;
            videoPlayout.Clear();
            decoder.Stop();
            decoderStarted = false;
            decoderRecovery.Reset();
        }
        wasConnected = connected;

        const auto recoveryNow = phonecast::core::DecoderRecoveryController::Clock::now();
        if (decoderRecovery.Ready(recoveryNow)) {
            const auto attempt = decoderRecovery.Attempts() + 1U;
            const auto restartBegan = std::chrono::steady_clock::now();
            decoderStarted = decoder.Start(streamWidth, streamHeight, error);
            decoderStartMillis = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - restartBegan).count();
            if (decoderStarted) {
                decoderRecovery.RecordAttempt(true, recoveryNow);
                ++decoderRecoverySuccesses;
                std::cout << "[decoder] Recovery attempt " << attempt
                          << " succeeded in " << decoderStartMillis << " ms.\n";
#ifndef _WIN32
                std::cout << "[decoder] Using native V4L2 device "
                          << decoder.DevicePath() << ".\n";
#endif
                std::string resyncError;
                if (!server.RequestVideoResync(resyncError))
                    std::cerr << "[decoder] Warning: " << resyncError << '\n';
            } else {
                decoderRecovery.RecordAttempt(false, recoveryNow);
                ++decoderRecoveryFailedAttempts;
                std::cerr << "[decoder] Recovery attempt " << attempt << " failed: "
                          << error << '\n';
                if (decoderRecovery.Exhausted())
                    std::cerr << "[decoder] Recovery exhausted after "
                              << phonecast::core::DecoderRecoveryController::MaximumAttempts()
                              << " attempts; waiting for a new stream configuration or reconnect.\n";
            }
        }

        Message audioMessage;
        for (unsigned i = 0; i < 12 && server.PopAudio(audioMessage); ++i) {
            using namespace phonecast::core::audio;
            if (audioMessage.type == MessageType::AudioConfig) {
                if (ParseConfiguration(audioMessage,audioEpoch)) {
                    audio.Configure(audioEpoch); captureAudioStatus = CaptureStatus::Active;
                    videoPlayout.Clear();
                }
            } else if (audioMessage.type == MessageType::AudioStatus) {
                std::uint64_t epoch = 0; CaptureStatus status{};
                if (ParseStatus(audioMessage,epoch,status)) {
                    captureAudioStatus = status;
                    if (status == CaptureStatus::Off || status == CaptureStatus::Error) {
                        audio.Reset(); audioEpoch = 0; videoPlayout.Clear();
                    }
                }
            } else {
                Block block;
                if (ParseBlock(audioMessage,block) && block.epoch == audioEpoch) audio.Enqueue(std::move(block));
            }
        }

        Message message;
        phonecast::core::VideoFrame latestFrame;
        std::chrono::microseconds queueAge{};
        const auto dispatchStarted = std::chrono::steady_clock::now();
        phonecast::core::DispatchBudget dispatchBudget(dispatchStarted);
        while (dispatchBudget.CanDispatch(std::chrono::steady_clock::now()) && server.Pop(message, &queueAge)) {
            dispatchBudget.Dispatched();
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
                videoPlayout.Clear();
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
                    firstConfigMillis = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - connectionStarted).count();
                    std::cout << "[diagnostics] first-config-ms=" << firstConfigMillis
                              << " dimensions=" << message.width << 'x' << message.height << '\n';
                    configReported = true;
                }
                decoderRecovery.Reset();
                const auto decoderStartBegan = std::chrono::steady_clock::now();
                decoderStarted = decoder.Start(message.width, message.height, error);
                decoderStartMillis = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - decoderStartBegan).count();
                std::cout << "[diagnostics] decoder-start-ms=" << decoderStartMillis << '\n';
                if (!decoderStarted) {
                    const auto startError = error;
                    beginDecoderRecovery("Initial decoder start failed: " + startError);
#ifndef _WIN32
                } else {
                    std::cout << "[decoder] Using native V4L2 device "
                              << decoder.DevicePath() << ".\n";
#endif
                }
                continue;
            }
            if (!decoderStarted) continue;
            std::vector<std::uint8_t> accessUnit;
            if ((message.flags & phonecast::core::protocol::MessageFlags::KeyFrame) != 0) {
                if (!keyFrameReported) {
                    firstKeyFrameMillis = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - connectionStarted).count();
                    std::cout << "[diagnostics] first-keyframe-ms=" << firstKeyFrameMillis << '\n';
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
                const auto submitError = error;
                beginDecoderRecovery("Decode submission failed: " + submitError);
                break;
            } else if (produced) {
                windowDecodeMillis += std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - decodeStarted).count();
                videoPlayout.Add(std::move(frame),std::chrono::steady_clock::now());
                ++windowDecodedFrames;
                // Give every decoded picture a presentation opportunity and
                // return to interaction before consuming more transport work.
                break;
            }
        }
        maximumDispatchMessages = std::max(maximumDispatchMessages,dispatchBudget.Count());
        maximumDispatchMillis = std::max(maximumDispatchMillis,std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - dispatchStarted).count());
        if (videoPlayout.Take(audio.AudibleTimestamp(),std::chrono::steady_clock::now(),latestFrame)) {
            const auto renderStarted = std::chrono::steady_clock::now();
            if (!renderer.SubmitFrame(latestFrame, error)) {
                std::cerr << error << '\n';
                break;
            }
            windowRenderMillis += std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - renderStarted).count();
            ++windowRenderedFrames;
            const auto presentedAt = std::chrono::steady_clock::now();
            if (previousPresentation != std::chrono::steady_clock::time_point{})
                maximumPresentationGapMillis = std::max(maximumPresentationGapMillis,
                    std::chrono::duration<double, std::milli>(presentedAt - previousPresentation).count());
            previousPresentation = presentedAt;
            if (!firstFrameReported) {
                firstSubmittedMillis = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - connectionStarted).count();
                std::cout << "[diagnostics] first-submitted-frame-ms=" << firstSubmittedMillis << '\n';
                firstFrameReported = true;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        maximumLoopMillis = std::max(maximumLoopMillis,
            std::chrono::duration<double, std::milli>(now - loopStarted).count());
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
            if (decoderStarted && decoderRecovery.ObserveWindow(received, windowDecodedFrames)) {
                beginDecoderRecovery(
                    "Watchdog observed three active receive windows with no decoded output.");
            }
            const double receiveFps = received / seconds;
            const double decodeFps = windowDecodedFrames / seconds;
            const double renderFps = windowRenderedFrames / seconds;
            const double bitrateMbps = bytes * 8.0 / seconds / 1'000'000.0;
            const double decodeMillis = windowDecodedFrames > 0
                ? windowDecodeMillis / windowDecodedFrames : 0.0;
            const double renderMillis = windowRenderedFrames > 0
                ? windowRenderMillis / windowRenderedFrames : 0.0;
            const double queueMillis = windowMessages > 0
                ? windowQueueMillis / windowMessages : 0.0;
            const auto audioStats = audio.Stats();
            const auto videoSyncStats = videoPlayout.Stats();
            const auto audioBytes = server.AudioReceivedBytes();
            const auto audioBitrateMbps = (audioBytes - previousAudioBytes) * 8.0 / seconds / 1000000.0;
            previousAudioBytes = audioBytes;
            if (audioStats.status != previousAudioOutputStatus) {
                std::cout << "[audio] " << audioStats.status << '\n';
                previousAudioOutputStatus = audioStats.status;
            }
            using phonecast::core::audio::CaptureStatus;
            const std::string audioMenuStatus = controls.Settings().audioMuted ? "MUTED" :
                captureAudioStatus == CaptureStatus::Error ? "CAPTURE ERROR" :
                captureAudioStatus == CaptureStatus::Silent ? "SILENT / POLICY BLOCKED" :
                captureAudioStatus == CaptureStatus::Off ? "OFF / VIDEO ONLY" :
                audioStats.status.rfind("Playing:",0) == 0 ? "PLAYING" : "OUTPUT UNAVAILABLE";
            settingsMenu.SetAudioStatus(audioMenuStatus);
            if (settingsMenu.IsOpen() && settingsMenu.View().title == "PHONE AUDIO")
                renderer.ShowSettingsMenu(settingsMenu.View(),error);
            const auto processStats = processSampler.Sample();
            phonecast::vr::VrPerformanceStats vrStats;
            const bool haveVrStats = renderer.GetPerformanceStats(vrStats);

            std::cout << "[diagnostics] connected=" << (connected ? "yes" : "no")
                      << " rx-fps=" << receiveFps
                      << " decode-fps=" << decodeFps
                      << " render-fps=" << renderFps
                      << " bitrate-mbps=" << bitrateMbps
                      << " decode-ms=" << decodeMillis
                      << " render-ms=" << renderMillis
                      << " queue-ms=" << queueMillis
                      << " queue-max-ms=" << maximumQueueMillis
                      << " queue-depth=" << serverStats.queueDepth
                      << " keyframes=" << keyFrames
                      << " dropped=" << dropped
                      << " resyncs=" << resyncs
                      << " decoder-recovery-triggers=" << decoderRecoveryTriggers
                      << " decoder-recovery-successes=" << decoderRecoverySuccesses
                      << " decoder-recovery-failed-attempts=" << decoderRecoveryFailedAttempts
                      << " audio-submitted=" << audioStats.submitted
                      << " audio-dropped=" << audioStats.dropped + server.AudioDropped()
                      << " audio-failures=" << audioStats.failures
                      << " audio-sequence-gaps=" << audioStats.sequenceGaps
                      << " audio-timestamp-gaps=" << audioStats.timestampGaps
                      << " audio-reanchors=" << audioStats.reanchors
                      << " audio-flushes=" << audioStats.flushes
                      << " audio-opens=" << audioStats.opens
                      << " audio-stale-drops=" << audioStats.staleDrops
                      << " audio-overflow-drops=" << audioStats.overflowDrops
                      << " audio-rejected-drops=" << audioStats.rejectedDrops
                      << " audio-max-open-ms=" << audioStats.maximumOpenMicros / 1000.0
                      << " audio-queued=" << audioStats.queued
                      << " audio-output-latency-ms=" << audioStats.latencyMicros / 1000.0
                      << " audio-bitrate-mbps=" << audioBitrateMbps
                      << " video-sync-queued=" << videoPlayout.Size()
                      << " video-sync-deadline-releases=" << videoSyncStats.deadlineReleases
                      << " video-sync-overflow-releases=" << videoSyncStats.overflowReleases
                      << " video-sync-coalesced=" << videoSyncStats.coalescedFrames
                      << " video-sync-last-hold-ms=" << videoSyncStats.lastHoldMicros / 1000.0
                      << " video-sync-skew-valid=" << (videoSyncStats.haveEstimatedSkew ? 1 : 0)
                      << " video-sync-estimated-skew-ms=" << videoSyncStats.estimatedSkewMicros / 1000.0
                      << " dispatch-max-messages=" << maximumDispatchMessages
                      << " dispatch-max-ms=" << maximumDispatchMillis
                      << " presentation-max-gap-ms=" << maximumPresentationGapMillis
                      << " loop-max-ms=" << maximumLoopMillis
                      << " process-cpu-percent=" << processStats.cpuPercent
                      << " working-set-mb=" << processStats.workingSetMegabytes;
            if (haveVrStats) {
                std::cout << " vr-total-gpu-ms=" << vrStats.totalRenderGpuMilliseconds
                          << " vr-compositor-gpu-ms=" << vrStats.compositorGpuMilliseconds
                          << " vr-compositor-cpu-ms=" << vrStats.compositorCpuMilliseconds
                          << " vr-dropped=" << vrStats.droppedFrames
                          << " vr-mispresented=" << vrStats.misPresentedFrames;
            }
            std::cout << '\n';

            if (performanceLog) {
                const double elapsed = std::chrono::duration<double>(now - sessionStarted).count();
                performanceLog << std::fixed << std::setprecision(3)
                    << elapsed << ',' << (connected ? 1 : 0) << ','
                    << streamWidth << ',' << streamHeight << ','
                    << receiveFps << ',' << decodeFps << ',' << renderFps << ','
                    << bitrateMbps << ',' << decodeMillis << ',' << renderMillis << ','
                    << queueMillis << ',' << maximumQueueMillis << ',' << serverStats.queueDepth << ','
                    << dropped << ',' << resyncs << ','
                    << (processStats.available ? processStats.cpuPercent : -1.0) << ','
                    << (processStats.available ? processStats.workingSetMegabytes : -1.0) << ','
                    << (processStats.available ? processStats.privateMegabytes : -1.0) << ','
                    << (haveVrStats ? static_cast<double>(vrStats.frameIndex) : -1.0) << ','
                    << (haveVrStats ? static_cast<double>(vrStats.framePresents) : -1.0) << ','
                    << (haveVrStats ? static_cast<double>(vrStats.misPresentedFrames) : -1.0) << ','
                    << (haveVrStats ? static_cast<double>(vrStats.droppedFrames) : -1.0) << ','
                    << (haveVrStats ? static_cast<double>(vrStats.reprojectionFlags) : -1.0) << ','
                    << (haveVrStats ? vrStats.totalRenderGpuMilliseconds : -1.0F) << ','
                    << (haveVrStats ? vrStats.compositorGpuMilliseconds : -1.0F) << ','
                    << (haveVrStats ? vrStats.compositorCpuMilliseconds : -1.0F) << ','
                    << (haveVrStats ? vrStats.clientFrameIntervalMilliseconds : -1.0F) << ','
                    << firstConfigMillis << ',' << firstKeyFrameMillis << ','
                    << decoderStartMillis << ',' << firstSubmittedMillis << ','
                    << decoderRecoveryTriggers << ',' << decoderRecoverySuccesses << ','
                    << decoderRecoveryFailedAttempts << ',' << audioStats.submitted << ','
                    << audioStats.dropped + server.AudioDropped() << ',' << audioStats.failures << ','
                    << audioStats.queued << ',' << audioStats.latencyMicros / 1000.0 << ',' << audioBitrateMbps << ','
                    << videoPlayout.Size() << ',' << videoSyncStats.deadlineReleases << ','
                    << videoSyncStats.overflowReleases << ',' << videoSyncStats.coalescedFrames << ','
                    << videoSyncStats.lastHoldMicros / 1000.0 << ',' << (videoSyncStats.haveEstimatedSkew ? 1 : 0) << ','
                    << videoSyncStats.estimatedSkewMicros / 1000.0 << ','
                    << maximumDispatchMessages << ',' << maximumDispatchMillis << ','
                    << maximumPresentationGapMillis << ',' << maximumLoopMillis << ','
                    << audioStats.sequenceGaps << ',' << audioStats.timestampGaps << ','
                    << audioStats.reanchors << ',' << audioStats.flushes << ',' << audioStats.opens << ','
                    << audioStats.staleDrops << ',' << audioStats.overflowDrops << ','
                    << audioStats.rejectedDrops << ',' << audioStats.maximumOpenMicros / 1000.0 << '\n';
                performanceLog.flush();
            }

            previousServerStats = serverStats;
            windowDecodedFrames = 0;
            windowRenderedFrames = 0;
            windowDecodeMillis = 0.0;
            windowRenderMillis = 0.0;
            windowQueueMillis = 0.0;
            maximumQueueMillis = 0.0;
            windowMessages = 0;
            maximumDispatchMessages = 0;
            maximumDispatchMillis = maximumPresentationGapMillis = maximumLoopMillis = 0.0;
            lastStats = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    server.Stop();
    audio.Stop();
    decoder.Stop();
    renderer.Stop();
    return EXIT_SUCCESS;
}
