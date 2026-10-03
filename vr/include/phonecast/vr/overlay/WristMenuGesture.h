#pragma once

#include <cstdint>

namespace phonecast::vr {

enum class WristMenuGestureAction {
    None,
    OpenLeft,
    OpenRight
};

struct WristMenuGestureResult {
    WristMenuGestureAction action{WristMenuGestureAction::None};
    float progress{0.0F};
};

struct WristMenuGestureObservation {
    bool leftFacing{false};
    bool rightFacing{false};
    bool menuVisible{false};
    std::uint64_t nowMilliseconds{0};
};

// Opens the control panel after a deliberate wrist-facing dwell. Both wrists
// must first be turned away, preventing immediate activation at startup. Once
// open, the panel remains available for laser interaction without requiring the
// user to hold the wrist pose.
class WristMenuGesture {
public:
    WristMenuGestureResult Update(const WristMenuGestureObservation& observation);
    void Reset();

private:
    bool armed_{false};
    int openingHand_{0};
    std::uint64_t openingSince_{0};
};

}  // namespace phonecast::vr
