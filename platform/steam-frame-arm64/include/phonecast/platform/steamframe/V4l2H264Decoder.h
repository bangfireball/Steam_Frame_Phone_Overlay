#pragma once

#include "phonecast/core/streaming/VideoFrame.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace phonecast::platform::steamframe {

// Stateful V4L2 M2M decoder for Steam Frame's native Qualcomm/iris video path.
// Decoded NV12 frames are converted to RGBA until a compositor-compatible
// dma-buf texture submission path is validated on hardware.
class V4l2H264Decoder {
public:
    explicit V4l2H264Decoder(std::string devicePath = {});
    ~V4l2H264Decoder();
    V4l2H264Decoder(const V4l2H264Decoder&) = delete;
    V4l2H264Decoder& operator=(const V4l2H264Decoder&) = delete;

    bool Start(std::uint32_t width, std::uint32_t height, std::string& error);
    bool Submit(const std::vector<std::uint8_t>& accessUnit,
                std::uint64_t timestampMicros, core::VideoFrame& frame,
                bool& producedFrame, std::string& error);
    void Stop() noexcept;

    [[nodiscard]] const std::string& DevicePath() const noexcept;

private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};

}  // namespace phonecast::platform::steamframe
