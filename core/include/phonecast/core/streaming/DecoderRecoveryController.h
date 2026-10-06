#pragma once

#include <chrono>
#include <cstdint>

namespace phonecast::core {

// Bounds decoder restart attempts and detects a decoder that is accepting input
// without producing output. Device-specific reset mechanics remain in the
// platform decoder and composition root.
class DecoderRecoveryController {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void Begin(TimePoint now) noexcept;
    [[nodiscard]] bool Ready(TimePoint now) const noexcept;
    void RecordAttempt(bool succeeded, TimePoint now) noexcept;
    void Reset() noexcept;

    // Samples are expected approximately once per second. A recovery is
    // requested only after three consecutive windows with active input and no
    // decoded output, avoiding resets for sparse/static screens.
    [[nodiscard]] bool ObserveWindow(std::uint64_t receivedFrames,
                                     std::uint64_t decodedFrames) noexcept;

    [[nodiscard]] bool Active() const noexcept { return active_; }
    [[nodiscard]] bool Exhausted() const noexcept { return exhausted_; }
    [[nodiscard]] std::uint32_t Attempts() const noexcept { return attempts_; }

    static constexpr std::uint32_t MaximumAttempts() noexcept { return 3; }

private:
    static constexpr std::uint64_t kMinimumActiveFrames = 10;
    static constexpr std::uint32_t kStallWindowLimit = 3;

    TimePoint nextAttempt_{};
    std::uint32_t attempts_{};
    std::uint32_t stalledWindows_{};
    bool active_{};
    bool exhausted_{};
};

}  // namespace phonecast::core
