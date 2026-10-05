#pragma once

#include <cstdint>
#include <memory>

namespace phonecast::platform::windows {

struct ProcessPerformanceStats {
    bool available{false};
    double cpuPercent{};
    double workingSetMegabytes{};
    double privateMegabytes{};
};

class ProcessPerformanceSampler {
public:
    ProcessPerformanceSampler();
    ~ProcessPerformanceSampler();
    ProcessPerformanceSampler(const ProcessPerformanceSampler&) = delete;
    ProcessPerformanceSampler& operator=(const ProcessPerformanceSampler&) = delete;

    ProcessPerformanceStats Sample() noexcept;

private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};

}  // namespace phonecast::platform::windows
