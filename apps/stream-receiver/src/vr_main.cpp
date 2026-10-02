#include "phonecast/core/logging/ConsoleLogger.h"
#include "phonecast/core/protocol/StreamProtocol.h"
#include "phonecast/platform/openvr/OpenVrOverlayRenderer.h"
#include "phonecast/platform/windows/MfH264Decoder.h"
#include "phonecast/platform/windows/TcpVideoServer.h"
#include "phonecast/vr/overlay/OverlayController.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
    std::uint64_t decodedFrames = 0;
    std::uint64_t receivedBytes = 0;
    std::uint64_t lastSequence = 0;
    std::uint64_t missingFrames = 0;
    double totalDecodeMillis = 0.0;
    auto statsStarted = std::chrono::steady_clock::now();
    auto lastStats = statsStarted;

    while (running && renderer.PumpEvents()) {
        OverlayAction action{};
        bool quit = false;
        if (PollControl(action, quit)) {
            controls.Apply(action);
            if (!renderer.ApplySettings(controls.Settings(), error) ||
                !renderer.SetVisible(controls.Visible(), error)) {
                std::cerr << error << '\n';
                running = false;
            }
        }
        if (quit) break;

        Message message;
        phonecast::core::VideoFrame latestFrame;
        bool haveFrame = false;
        while (server.Pop(message)) {
            receivedBytes += message.payload.size();
            if (message.type == MessageType::VideoConfig) {
                codecConfig = std::move(message.payload);
                decoderStarted = decoder.Start(message.width, message.height, error);
                if (!decoderStarted) std::cerr << error << '\n';
                continue;
            }
            if (!decoderStarted) continue;
            if (decodedFrames > 0 && message.sequence > lastSequence + 1) {
                missingFrames += message.sequence - lastSequence - 1;
            }
            lastSequence = message.sequence;
            std::vector<std::uint8_t> accessUnit;
            if ((message.flags & phonecast::core::protocol::MessageFlags::KeyFrame) != 0) {
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
                totalDecodeMillis += std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - decodeStarted).count();
                latestFrame = std::move(frame);
                haveFrame = true;
                ++decodedFrames;
            }
        }
        if (haveFrame && !renderer.SubmitFrame(latestFrame, error)) {
            std::cerr << error << '\n';
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - lastStats >= std::chrono::seconds(1)) {
            const double seconds = std::chrono::duration<double>(now - statsStarted).count();
            std::cout << "[diagnostics] connected=" << (server.Connected() ? "yes" : "no")
                      << " fps=" << (seconds > 0 ? decodedFrames / seconds : 0.0)
                      << " bitrate-mbps=" << (seconds > 0 ? receivedBytes * 8.0 / seconds / 1'000'000.0 : 0.0)
                      << " decode-ms=" << (decodedFrames > 0 ? totalDecodeMillis / decodedFrames : 0.0)
                      << " dropped=" << (server.DroppedMessages() + missingFrames) << '\n';
            lastStats = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    server.Stop();
    decoder.Stop();
    renderer.Stop();
    return EXIT_SUCCESS;
}
