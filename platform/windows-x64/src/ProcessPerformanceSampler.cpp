#include "phonecast/platform/windows/ProcessPerformanceSampler.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

#include <chrono>
#include <memory>

namespace phonecast::platform::windows {
namespace {

std::uint64_t FileTimeValue(const FILETIME& value) noexcept {
    ULARGE_INTEGER integer{};
    integer.LowPart = value.dwLowDateTime;
    integer.HighPart = value.dwHighDateTime;
    return integer.QuadPart;
}

}  // namespace

struct ProcessPerformanceSampler::Implementation {
    HANDLE process{GetCurrentProcess()};
    std::uint64_t previousProcessTicks{};
    std::chrono::steady_clock::time_point previousWall{};
    unsigned int processorCount{1};
    bool haveCpuBaseline{false};
};

ProcessPerformanceSampler::ProcessPerformanceSampler()
    : implementation_(std::make_unique<Implementation>()) {
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    implementation_->processorCount = system.dwNumberOfProcessors > 0
        ? system.dwNumberOfProcessors : 1U;
}

ProcessPerformanceSampler::~ProcessPerformanceSampler() = default;

ProcessPerformanceStats ProcessPerformanceSampler::Sample() noexcept {
    ProcessPerformanceStats stats;
    auto& state = *implementation_;

    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    if (GetProcessMemoryInfo(state.process,
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory))) {
        constexpr double bytesPerMegabyte = 1024.0 * 1024.0;
        stats.workingSetMegabytes = memory.WorkingSetSize / bytesPerMegabyte;
        stats.privateMegabytes = memory.PrivateUsage / bytesPerMegabyte;
        stats.available = true;
    }

    FILETIME creation{}, exit{}, kernel{}, user{};
    const auto now = std::chrono::steady_clock::now();
    if (!GetProcessTimes(state.process, &creation, &exit, &kernel, &user)) return stats;
    const std::uint64_t processTicks = FileTimeValue(kernel) + FileTimeValue(user);
    if (state.haveCpuBaseline) {
        const double wallSeconds = std::chrono::duration<double>(now - state.previousWall).count();
        if (wallSeconds > 0.0 && processTicks >= state.previousProcessTicks) {
            const double processSeconds =
                static_cast<double>(processTicks - state.previousProcessTicks) / 10'000'000.0;
            stats.cpuPercent = processSeconds * 100.0 /
                (wallSeconds * static_cast<double>(state.processorCount));
            stats.available = true;
        }
    }
    state.previousProcessTicks = processTicks;
    state.previousWall = now;
    state.haveCpuBaseline = true;
    return stats;
}

}  // namespace phonecast::platform::windows
