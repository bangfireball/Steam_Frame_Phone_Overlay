#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>

namespace phonecast::core::audio {
// Estimated audible media time, never a cross-device wall-clock subtraction.
// Interpolate only through samples already submitted to the device. Observations
// correct the mapping by at most 2 ms, rather than following raw latency jitter.
// A missing clock expires after 100 ms; timeline changes explicitly Reset it.
class MediaClock {
public:
    using Clock = std::chrono::steady_clock;
    void Reset() { valid_ = false; lastReturned_ = 0; }
    void Observe(std::uint64_t audible, std::uint64_t submittedEnd, Clock::time_point now) {
        if (audible == 0 || audible > submittedEnd) return;
        if (valid_ && submittedEnd < end_) Reset();
        const auto previous = Timestamp(now);
        end_ = submittedEnd;
        if (previous == 0) anchor_ = audible;
        else if (audible >= previous) anchor_ = previous + std::min<std::uint64_t>(audible-previous,2000);
        else anchor_ = previous - std::min<std::uint64_t>(previous-audible,2000);
        anchor_ = std::clamp(anchor_,lastReturned_,end_);
        observed_ = now; valid_ = true;
    }
    std::uint64_t Timestamp(Clock::time_point now) const {
        if (!valid_ || now < observed_ || now-observed_ >= std::chrono::milliseconds(100)) return 0;
        const auto elapsed = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(now-observed_).count());
        // Overflow-safe saturation at the submitted sample boundary.
        const auto value = anchor_ + std::min(elapsed,end_-anchor_);
        lastReturned_ = std::max(lastReturned_,value);
        return lastReturned_;
    }
private:
    bool valid_{};
    std::uint64_t anchor_{}, end_{};
    mutable std::uint64_t lastReturned_{};
    Clock::time_point observed_{};
};
} // namespace phonecast::core::audio
