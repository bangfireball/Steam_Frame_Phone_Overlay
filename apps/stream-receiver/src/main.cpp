#include "phonecast/core/protocol/StreamProtocol.h"
#include "phonecast/core/streaming/VideoFrame.h"
#include "phonecast/platform/windows/DesktopPreview.h"
#include "phonecast/platform/windows/MfH264Decoder.h"
#include "phonecast/platform/windows/TcpVideoServer.h"

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

void PrintUsage() {
    std::cout << "Usage: phonecast-stream-receiver --pair-code NNNNNN [--port N]\n";
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

    phonecast::platform::windows::DesktopPreview preview;
    std::string error;
    if (!preview.Start(error)) {
        std::cerr << error << '\n';
        return EXIT_FAILURE;
    }
    phonecast::platform::windows::TcpVideoServer server(pairCode, port);
    if (!server.Start(error)) {
        std::cerr << error << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "PhoneCast desktop receiver listening on port " << port
              << ". Pairing code: " << pairCode << '\n';

    phonecast::platform::windows::MfH264Decoder decoder;
    bool decoderStarted = false;
    std::vector<std::uint8_t> codecConfig;
    std::uint32_t streamWidth = 0;
    std::uint32_t streamHeight = 0;
    std::uint64_t windowDecodedFrames = 0;
    double windowDecodeMillis = 0.0;
    double windowQueueMillis = 0.0;
    std::uint64_t windowMessages = 0;
    phonecast::platform::windows::VideoServerStats previousServerStats{};
    auto lastTitle = std::chrono::steady_clock::now();
    auto connectionStarted = lastTitle;
    bool wasConnected = false;
    bool firstFrameReported = false;

    while (preview.PumpEvents()) {
        const bool connected = server.Connected();
        if (connected && !wasConnected) {
            connectionStarted = std::chrono::steady_clock::now();
            firstFrameReported = false;
        }
        wasConnected = connected;

        Message message;
        std::chrono::microseconds queueAge{};
        while (server.Pop(message, &queueAge)) {
            windowQueueMillis += queueAge.count() / 1000.0;
            ++windowMessages;
            if (message.type == MessageType::VideoConfig) {
                codecConfig = std::move(message.payload);
                streamWidth = message.width;
                streamHeight = message.height;
                decoderStarted = decoder.Start(streamWidth, streamHeight, error);
                if (!decoderStarted) std::cerr << error << '\n';
                continue;
            }
            if (!decoderStarted) continue;
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
                windowDecodeMillis += std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - decodeStarted).count();
                preview.Present(frame);
                ++windowDecodedFrames;
                if (!firstFrameReported) {
                    const double startupMillis = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - connectionStarted).count();
                    std::cout << "First decoded frame after " << startupMillis << " ms.\n";
                    firstFrameReported = true;
                }
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - lastTitle >= std::chrono::seconds(1)) {
            const double seconds = std::chrono::duration<double>(now - lastTitle).count();
            const auto serverStats = server.Stats();
            const auto received = serverStats.receivedFrames - previousServerStats.receivedFrames;
            const auto bytes = serverStats.receivedBytes - previousServerStats.receivedBytes;
            const auto dropped = serverStats.droppedFrames - previousServerStats.droppedFrames;
            const auto resyncs = serverStats.resyncRequests - previousServerStats.resyncRequests;
            const double fps = windowDecodedFrames / seconds;
            const double megabits = bytes * 8.0 / seconds / 1'000'000.0;
            preview.SetTitle("PhoneCast | " + server.Status() + " | " +
                std::to_string(streamWidth) + "x" + std::to_string(streamHeight) + " | rx " +
                std::to_string(received / seconds).substr(0, 4) + " | decode " +
                std::to_string(fps).substr(0, 4) + " FPS | " +
                std::to_string(megabits).substr(0, 4) + " Mbps | decode " +
                std::to_string(windowDecodedFrames > 0 ? windowDecodeMillis / windowDecodedFrames : 0.0).substr(0, 4) +
                " ms | queue " +
                std::to_string(windowMessages > 0 ? windowQueueMillis / windowMessages : 0.0).substr(0, 4) +
                " ms | dropped " + std::to_string(dropped) +
                " | resync " + std::to_string(resyncs));
            previousServerStats = serverStats;
            windowDecodedFrames = 0;
            windowDecodeMillis = 0.0;
            windowQueueMillis = 0.0;
            windowMessages = 0;
            lastTitle = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    server.Stop();
    decoder.Stop();
    preview.Stop();
    return EXIT_SUCCESS;
}
