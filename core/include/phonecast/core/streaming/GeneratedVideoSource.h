#pragma once

#include "phonecast/core/streaming/IVideoSource.h"

#include <cstdint>

namespace phonecast::core {

class GeneratedVideoSource final : public IVideoSource {
public:
    GeneratedVideoSource(std::uint32_t width, std::uint32_t height);
    bool NextFrame(VideoFrame& frame, std::string& error) override;

private:
    std::uint32_t width_;
    std::uint32_t height_;
    std::uint64_t sequence_{0};
};

}  // namespace phonecast::core
