#pragma once

#include <cstdint>

namespace phonecast::core {

struct PointerEvent {
    enum class Type { Down, Move, Up, Scroll, Back };
    Type type{Type::Move};
    float normalizedX{};
    float normalizedY{};
    float scrollDelta{};
    std::uint64_t sequence{};
};

class IInputProvider {
public:
    virtual ~IInputProvider() = default;
    virtual bool Poll(PointerEvent& event) = 0;
};

}  // namespace phonecast::core
