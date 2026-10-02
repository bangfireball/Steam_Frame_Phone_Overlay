#pragma once

#include <string_view>

namespace phonecast::core {

enum class LogLevel { Debug, Info, Warning, Error };

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void Log(LogLevel level, std::string_view subsystem, std::string_view message) = 0;
};

}  // namespace phonecast::core
