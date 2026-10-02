#include "phonecast/apps/Receiver.h"
#include "phonecast/core/config/AppConfig.h"
#include "phonecast/core/logging/ConsoleLogger.h"
#include "phonecast/core/streaming/GeneratedVideoSource.h"
#include "phonecast/platform/openvr/OpenVrOverlayRenderer.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {
std::atomic_bool g_running{true};
void OnSignal(int) { g_running.store(false); }
}

int main(int argc, char** argv) {
    std::vector<std::string> arguments;
    for (int index = 1; index < argc; ++index) arguments.emplace_back(argv[index]);
    const auto parsed = phonecast::core::ParseCommandLine(arguments);
    if (parsed.status == phonecast::core::ParseStatus::Help) {
        std::cout << parsed.message;
        return EXIT_SUCCESS;
    }
    if (parsed.status == phonecast::core::ParseStatus::Error) {
        std::cerr << "[error] " << parsed.message << '\n' << phonecast::core::ReceiverUsage();
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);
    phonecast::core::ConsoleLogger logger;
    logger.Log(phonecast::core::LogLevel::Info, "application", "Starting PhoneCast VR receiver.");

    phonecast::core::GeneratedVideoSource source(parsed.config.textureWidth,
                                                  parsed.config.textureHeight);
    phonecast::platform::openvr::OpenVrOverlayRenderer renderer(logger, argv[0]);
    phonecast::apps::Receiver receiver(source, renderer, logger);
    phonecast::vr::OverlaySettings overlaySettings;
    overlaySettings.widthMeters = parsed.config.overlayWidthMeters;
    overlaySettings.distanceMeters = parsed.config.overlayDistanceMeters;
    overlaySettings.alpha = parsed.config.overlayAlpha;

    std::string error;
    if (!receiver.Start(overlaySettings, error)) return EXIT_FAILURE;

    const auto started = std::chrono::steady_clock::now();
    const auto framePeriod = std::chrono::microseconds(1000000 / parsed.config.framesPerSecond);
    auto nextFrame = started;
    bool failed = false;
    while (g_running.load()) {
        if (!receiver.Tick(error)) {
            failed = !error.empty();
            break;
        }
        if (parsed.config.durationSeconds > 0 &&
            std::chrono::steady_clock::now() - started >=
                std::chrono::seconds(parsed.config.durationSeconds)) break;
        nextFrame += framePeriod;
        std::this_thread::sleep_until(nextFrame);
    }

    const auto submitted = receiver.SubmittedFrames();
    receiver.Stop();
    logger.Log(phonecast::core::LogLevel::Info, "application",
               "Submitted " + std::to_string(submitted) + " generated frames.");
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
