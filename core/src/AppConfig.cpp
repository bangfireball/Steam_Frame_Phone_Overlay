#include "phonecast/core/config/AppConfig.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace phonecast::core {
namespace {

bool ParseUnsigned(const std::string& text, std::uint32_t minimum, std::uint32_t maximum,
                   std::uint32_t& value) {
    if (text.empty() || text.front() == '-') return false;
    char* end = nullptr;
    errno = 0;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0' || parsed < minimum ||
        parsed > maximum || parsed > std::numeric_limits<std::uint32_t>::max()) return false;
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool ParseFloat(const std::string& text, float minimum, float maximum, float& value) {
    if (text.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(text.c_str(), &end);
    if (errno != 0 || end == text.c_str() || *end != '\0' || !std::isfinite(parsed) ||
        parsed < minimum || parsed > maximum) {
        return false;
    }
    value = parsed;
    return true;
}

ParseResult Error(std::string message) {
    ParseResult result;
    result.status = ParseStatus::Error;
    result.message = std::move(message);
    return result;
}

}  // namespace

std::string ReceiverUsage() {
    return "Usage: phonecast-receiver [options]\n"
           "  --duration-seconds N  Stop automatically (0 means run until quit)\n"
           "  --width N             Generated texture width (64-4096)\n"
           "  --height N            Generated texture height (64-4096)\n"
           "  --fps N               Update rate (1-120)\n"
           "  --overlay-width M     Physical overlay width in metres (0.1-5.0)\n"
           "  --distance M          HMD-relative distance in metres (0.2-10.0)\n"
           "  --alpha A             Overlay opacity (0.0-1.0)\n"
           "  --help                 Show this message\n";
}

ParseResult ParseCommandLine(const std::vector<std::string>& arguments) {
    ParseResult result;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string& option = arguments[index];
        if (option == "--help" || option == "-h") {
            result.status = ParseStatus::Help;
            result.message = ReceiverUsage();
            return result;
        }
        if (index + 1 >= arguments.size()) return Error("Missing value for " + option + '.');
        const std::string& value = arguments[++index];
        bool valid = false;
        if (option == "--duration-seconds") {
            valid = ParseUnsigned(value, 0, 86400, result.config.durationSeconds);
        } else if (option == "--width") {
            valid = ParseUnsigned(value, 64, 4096, result.config.textureWidth);
        } else if (option == "--height") {
            valid = ParseUnsigned(value, 64, 4096, result.config.textureHeight);
        } else if (option == "--fps") {
            valid = ParseUnsigned(value, 1, 120, result.config.framesPerSecond);
        } else if (option == "--overlay-width") {
            valid = ParseFloat(value, 0.1F, 5.0F, result.config.overlayWidthMeters);
        } else if (option == "--distance") {
            valid = ParseFloat(value, 0.2F, 10.0F, result.config.overlayDistanceMeters);
        } else if (option == "--alpha") {
            valid = ParseFloat(value, 0.0F, 1.0F, result.config.overlayAlpha);
        } else {
            return Error("Unknown option: " + option + '.');
        }
        if (!valid) return Error("Invalid value for " + option + ": " + value + '.');
    }
    return result;
}

}  // namespace phonecast::core
