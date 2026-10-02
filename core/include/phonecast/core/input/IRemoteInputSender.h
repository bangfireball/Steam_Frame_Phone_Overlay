#pragma once

#include "phonecast/core/input/IInputProvider.h"

#include <string>

namespace phonecast::core {

class IRemoteInputSender {
public:
    virtual ~IRemoteInputSender() = default;
    virtual bool Send(const PointerEvent& event, std::string& error) = 0;
};

}  // namespace phonecast::core
