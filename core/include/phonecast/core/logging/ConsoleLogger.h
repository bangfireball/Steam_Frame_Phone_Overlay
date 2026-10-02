#pragma once

#include "phonecast/core/logging/ILogger.h"

#include <mutex>

namespace phonecast::core {

class ConsoleLogger final : public ILogger {
public:
    void Log(LogLevel level, std::string_view subsystem, std::string_view message) override;

private:
    std::mutex mutex_;
};

}  // namespace phonecast::core
