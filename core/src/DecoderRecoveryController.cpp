#include "phonecast/core/streaming/DecoderRecoveryController.h"

namespace phonecast::core {

void DecoderRecoveryController::Begin(TimePoint now) noexcept {
    if (active_) return;
    attempts_ = 0;
    stalledWindows_ = 0;
    exhausted_ = false;
    active_ = true;
    nextAttempt_ = now + std::chrono::milliseconds(100);
}

bool DecoderRecoveryController::Ready(TimePoint now) const noexcept {
    return active_ && now >= nextAttempt_;
}

void DecoderRecoveryController::RecordAttempt(bool succeeded, TimePoint now) noexcept {
    if (!active_) return;
    ++attempts_;
    if (succeeded) {
        active_ = false;
        exhausted_ = false;
        stalledWindows_ = 0;
        return;
    }
    if (attempts_ >= MaximumAttempts()) {
        active_ = false;
        exhausted_ = true;
        return;
    }
    const auto delay = std::chrono::milliseconds(250U << (attempts_ - 1U));
    nextAttempt_ = now + delay;
}

void DecoderRecoveryController::Reset() noexcept {
    nextAttempt_ = {};
    attempts_ = 0;
    stalledWindows_ = 0;
    active_ = false;
    exhausted_ = false;
}

bool DecoderRecoveryController::ObserveWindow(std::uint64_t receivedFrames,
                                               std::uint64_t decodedFrames) noexcept {
    if (active_ || exhausted_) return false;
    if (receivedFrames < kMinimumActiveFrames || decodedFrames > 0U) {
        stalledWindows_ = 0;
        return false;
    }
    ++stalledWindows_;
    if (stalledWindows_ < kStallWindowLimit) return false;
    stalledWindows_ = 0;
    return true;
}

}  // namespace phonecast::core
