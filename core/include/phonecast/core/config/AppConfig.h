#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace phonecast::core {

struct AppConfig {
    std::uint32_t textureWidth{640};
    std::uint32_t textureHeight{360};
    std::uint32_t framesPerSecond{30};
    std::uint32_t durationSeconds{0};
    float overlayWidthMeters{0.65F};
    float overlayDistanceMeters{1.0F};
    float overlayAlpha{1.0F};
};

enum class ParseStatus { Success, Help, Error };

struct ParseResult {
    ParseStatus status{ParseStatus::Success};
    AppConfig config{};
    std::string message;
};

ParseResult ParseCommandLine(const std::vector<std::string>& arguments);
std::string ReceiverUsage();

}  // namespace phonecast::core
