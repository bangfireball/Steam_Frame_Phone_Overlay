#include "phonecast/vr/overlay/WristMenuGesture.h"

#include <algorithm>

namespace phonecast::vr {
namespace {
constexpr std::uint64_t kOpenDwellMilliseconds = 3000;
}

WristMenuGestureResult WristMenuGesture::Update(
        const WristMenuGestureObservation& observation) {
    WristMenuGestureResult result;
    if (observation.menuVisible) {
        armed_ = false;
        openingHand_ = 0;
        openingSince_ = 0;
        return result;
    }
    if (!observation.leftFacing && !observation.rightFacing) {
        armed_ = true;
        openingHand_ = 0;
        openingSince_ = 0;
        return result;
    }
    if (!armed_) return result;

    const int hand = observation.leftFacing && !observation.rightFacing ? -1 :
                     observation.rightFacing && !observation.leftFacing ? 1 : 0;
    if (hand == 0) {
        openingHand_ = 0;
        openingSince_ = 0;
        return result;
    }
    if (openingHand_ != hand) {
        openingHand_ = hand;
        openingSince_ = observation.nowMilliseconds;
        return result;
    }

    const auto elapsed = observation.nowMilliseconds - openingSince_;
    result.progress = std::min(1.0F, static_cast<float>(elapsed) /
        static_cast<float>(kOpenDwellMilliseconds));
    if (elapsed < kOpenDwellMilliseconds) return result;

    armed_ = false;
    openingHand_ = 0;
    openingSince_ = 0;
    result.action = hand < 0 ? WristMenuGestureAction::OpenLeft
                             : WristMenuGestureAction::OpenRight;
    return result;
}

void WristMenuGesture::Reset() {
    armed_ = false;
    openingHand_ = 0;
    openingSince_ = 0;
}

}  // namespace phonecast::vr
