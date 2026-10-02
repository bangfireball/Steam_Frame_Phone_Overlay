#pragma once

#include "phonecast/core/streaming/VideoFrame.h"

#include <cstdint>
#include <string>
#include <vector>

namespace phonecast::core {

struct EncodedVideoPacket {
    std::uint64_t sequence{};
    std::vector<std::uint8_t> bytes;
};

class IVideoDecoder {
public:
    virtual ~IVideoDecoder() = default;
    virtual bool Decode(const EncodedVideoPacket& packet, VideoFrame& frame, std::string& error) = 0;
    virtual void Reset() noexcept = 0;
};

}  // namespace phonecast::core
