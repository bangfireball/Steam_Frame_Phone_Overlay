#include "phonecast/core/logging/ConsoleLogger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

namespace phonecast::core {
namespace {

const char* LevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "debug";
        case LogLevel::Info: return "info";
        case LogLevel::Warning: return "warning";
        case LogLevel::Error: return "error";
    }
    return "unknown";
}

}  // namespace

void ConsoleLogger::Log(LogLevel level, std::string_view subsystem, std::string_view message) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif

    std::lock_guard<std::mutex> lock(mutex_);
    std::ostream& stream = level == LogLevel::Error ? std::cerr : std::cout;
    stream << std::put_time(&local, "%Y-%m-%dT%H:%M:%S") << " [" << LevelName(level)
           << "] [" << subsystem << "] " << message << '\n';
}

}  // namespace phonecast::core
