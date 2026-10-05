#include "phonecast/platform/steamframe/ProcessPerformanceSampler.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

namespace phonecast::platform::steamframe {

struct ProcessPerformanceSampler::Implementation {
    std::uint64_t previousTicks{};
    std::chrono::steady_clock::time_point previousWall{};
    long ticksPerSecond{sysconf(_SC_CLK_TCK)};
    unsigned int processorCount{std::max(1U, std::thread::hardware_concurrency())};
    bool haveCpuBaseline{};
};

ProcessPerformanceSampler::ProcessPerformanceSampler()
    : implementation_(std::make_unique<Implementation>()) {}
ProcessPerformanceSampler::~ProcessPerformanceSampler() = default;

ProcessPerformanceStats ProcessPerformanceSampler::Sample() noexcept {
    ProcessPerformanceStats stats;
    auto& state = *implementation_;

    std::ifstream status("/proc/self/status");
    std::string key;
    double value = 0.0;
    std::string unit;
    while (status >> key) {
        if (key == "VmRSS:" || key == "RssAnon:") {
            status >> value >> unit;
            if (key == "VmRSS:") stats.workingSetMegabytes = value / 1024.0;
            else stats.privateMegabytes = value / 1024.0;
            stats.available = true;
        } else {
            std::string ignored;
            std::getline(status, ignored);
        }
    }

    std::ifstream stat("/proc/self/stat");
    std::string field;
    std::uint64_t userTicks = 0;
    std::uint64_t systemTicks = 0;
    try {
        for (int index = 1; stat >> field; ++index) {
            if (index == 14) userTicks = std::stoull(field);
            if (index == 15) {
                systemTicks = std::stoull(field);
                break;
            }
        }
    } catch (...) {
        stat.setstate(std::ios::failbit);
    }
    const auto now = std::chrono::steady_clock::now();
    const std::uint64_t processTicks = userTicks + systemTicks;
    if (stat && state.haveCpuBaseline && state.ticksPerSecond > 0 &&
        processTicks >= state.previousTicks) {
        const double wallSeconds =
            std::chrono::duration<double>(now - state.previousWall).count();
        if (wallSeconds > 0.0) {
            const double processSeconds = static_cast<double>(
                processTicks - state.previousTicks) / state.ticksPerSecond;
            stats.cpuPercent = processSeconds * 100.0 /
                (wallSeconds * static_cast<double>(state.processorCount));
            stats.available = true;
        }
    }
    state.previousTicks = processTicks;
    state.previousWall = now;
    state.haveCpuBaseline = stat.good();
    return stats;
}

}  // namespace phonecast::platform::steamframe
