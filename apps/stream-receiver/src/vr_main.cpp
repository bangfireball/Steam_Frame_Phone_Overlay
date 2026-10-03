#include "phonecast/core/logging/ConsoleLogger.h"
#include "phonecast/core/protocol/StreamProtocol.h"
#include "phonecast/platform/openvr/OpenVrOverlayRenderer.h"
#include "phonecast/platform/windows/MfH264Decoder.h"
#include "phonecast/platform/windows/TcpVideoServer.h"
#include "phonecast/vr/overlay/OverlayController.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
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
    std::cout << "Usage: phonecast-vr-stream-receiver --pair-code NNNNNN [--port N]\n\n"
              << "Global controls (hold Ctrl+Alt):\n"
              << "  P          Show/hide\n"
              << "  + / -      Scale up/down\n"
              << "  Arrows     Move overlay\n"
              << "  PageUp/Down  Move nearer/farther\n"
              << "  ] / [      Increase/decrease opacity\n"
              << "  Home       Reset appearance and position\n"
              << "  End        Quit PhoneCast\n";
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

bool PollControl(OverlayAction& action, bool& quit) {
    quit = false;
    const bool modified = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
                          (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    const struct Binding { int key; OverlayAction action; } bindings[] = {
        {'P', OverlayAction::ToggleVisibility},
        {VK_OEM_PLUS, OverlayAction::ScaleUp}, {VK_ADD, OverlayAction::ScaleUp},
        {VK_OEM_MINUS, OverlayAction::ScaleDown}, {VK_SUBTRACT, OverlayAction::ScaleDown},
        {VK_LEFT, OverlayAction::MoveLeft}, {VK_RIGHT, OverlayAction::MoveRight},
        {VK_UP, OverlayAction::MoveUp}, {VK_DOWN, OverlayAction::MoveDown},
        {VK_PRIOR, OverlayAction::DistanceNearer}, {VK_NEXT, OverlayAction::DistanceFarther},
        {VK_OEM_6, OverlayAction::OpacityUp}, {VK_OEM_4, OverlayAction::OpacityDown},
        {VK_HOME, OverlayAction::Reset},
    };
    bool found = false;
    for (const auto& binding : bindings) {
        if (Pressed(binding.key) && modified && !found) {
            action = binding.action;
            found = true;
        }
    }
    if (Pressed(VK_END) && modified) quit = true;
    return found;
}
}  // namespace

int main(int argc, char** argv) {
    std::string pairCode;
    std::uint16_t port = 49321;
    for (int index = 1; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--help" || option == "-h") {
            PrintUsage();
            return EXIT_SUCCESS;
        }
        if (index + 1 >= argc) {
            PrintUsage();
            return EXIT_FAILURE;
        }
        const std::string value = argv[++index];
        if (option == "--pair-code") pairCode = value;
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
    phonecast::vr::OverlayController controls;
    std::string error;
    if (!renderer.Start(controls.Settings(), error)) {
        std::cerr << error << '\n';
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

    while (running && renderer.PumpEvents()) {
        OverlayAction action{};
        bool quit = false;
        if (PollControl(action, quit)) {
            controls.Apply(action);
            const auto displayedSettings = SettingsForStream(
                controls.Settings(), streamWidth, streamHeight);
            if (!renderer.ApplySettings(displayedSettings, error) ||
                !renderer.SetVisible(controls.Visible(), error)) {
                std::cerr << error << '\n';
                running = false;
            }
        }
        if (quit) break;

        const bool connected = server.Connected();
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
            if (message.type == MessageType::VideoConfig) {
                codecConfig = std::move(message.payload);
                streamWidth = message.width;
                streamHeight = message.height;
                const auto displayedSettings = SettingsForStream(
                    controls.Settings(), streamWidth, streamHeight);
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
