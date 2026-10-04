#pragma once

#include <cstdint>

namespace phonecast::core {

struct PointerEvent {
    enum class Type : std::uint8_t { Down = 1, Move = 2, Up = 3, Scroll = 4, Back = 5 };
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
