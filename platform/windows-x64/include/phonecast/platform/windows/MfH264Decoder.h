#pragma once

#include "phonecast/core/streaming/VideoFrame.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace phonecast::platform::windows {

class MfH264Decoder {
public:
    MfH264Decoder();
    ~MfH264Decoder();
    MfH264Decoder(const MfH264Decoder&) = delete;
    MfH264Decoder& operator=(const MfH264Decoder&) = delete;

    bool Start(std::uint32_t width, std::uint32_t height, std::string& error);
    bool Submit(const std::vector<std::uint8_t>& accessUnit, std::uint64_t timestampMicros,
                core::VideoFrame& frame, bool& producedFrame, std::string& error);
    void Stop() noexcept;

private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};

}  // namespace phonecast::platform::windows
