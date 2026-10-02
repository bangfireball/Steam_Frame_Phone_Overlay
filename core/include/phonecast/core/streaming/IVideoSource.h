#pragma once

#include "phonecast/core/streaming/VideoFrame.h"

#include <string>

namespace phonecast::core {

class IVideoSource {
public:
    virtual ~IVideoSource() = default;
    virtual bool NextFrame(VideoFrame& frame, std::string& error) = 0;
};

}  // namespace phonecast::core
