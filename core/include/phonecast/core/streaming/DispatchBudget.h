#pragma once

#include <chrono>
#include <cstddef>

namespace phonecast::core {
// Cooperative limit: one decoder/control call may exceed the time budget.
// Check before popping, never discard dependent H.264 packets to meet it.
class DispatchBudget {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::size_t MaximumMessages = 4;
    static constexpr auto MaximumTime = std::chrono::milliseconds(8);
    explicit DispatchBudget(Clock::time_point start) : start_(start) {}
    bool CanDispatch(Clock::time_point now) const {
        return count_ < MaximumMessages && now - start_ < MaximumTime;
    }
    void Dispatched() { ++count_; }
    std::size_t Count() const { return count_; }
private:
    Clock::time_point start_;
    std::size_t count_{};
};
} // namespace phonecast::core
