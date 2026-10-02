#include "phonecast/core/streaming/GeneratedVideoSource.h"

#include <algorithm>
#include <limits>

namespace phonecast::core {

GeneratedVideoSource::GeneratedVideoSource(std::uint32_t width, std::uint32_t height)
    : width_(width), height_(height) {}

bool GeneratedVideoSource::NextFrame(VideoFrame& frame, std::string& error) {
    if (width_ == 0 || height_ == 0 ||
        static_cast<std::uint64_t>(width_) * height_ >
            std::numeric_limits<std::size_t>::max() / 4U) {
        error = "Invalid generated frame dimensions.";
        return false;
    }

    frame.width = width_;
    frame.height = height_;
    frame.format = PixelFormat::Rgba8;
    frame.sequence = sequence_++;
    frame.pixels.resize(static_cast<std::size_t>(width_) * height_ * 4U);

    const std::uint32_t bandWidth = std::max(1U, width_ / 8U);
    const std::uint32_t bandStart = static_cast<std::uint32_t>((frame.sequence * 7U) % width_);
    for (std::uint32_t y = 0; y < height_; ++y) {
        for (std::uint32_t x = 0; x < width_; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * width_ + x) * 4U;
            const std::uint32_t wrappedDistance = (x + width_ - bandStart) % width_;
            const bool movingBand = wrappedDistance < bandWidth;
            frame.pixels[offset] = static_cast<std::uint8_t>(20U + (x * 80U / width_));
            frame.pixels[offset + 1U] = static_cast<std::uint8_t>(30U + (y * 100U / height_));
            frame.pixels[offset + 2U] = movingBand ? 245U : 90U;
            frame.pixels[offset + 3U] = 255U;
        }
    }

    // A bright progress line makes frame updates and stalls immediately visible.
    const std::uint32_t lineY = static_cast<std::uint32_t>((frame.sequence * 3U) % height_);
    for (std::uint32_t x = 0; x < width_; ++x) {
        const auto offset = (static_cast<std::size_t>(lineY) * width_ + x) * 4U;
        frame.pixels[offset] = 55U;
        frame.pixels[offset + 1U] = 210U;
        frame.pixels[offset + 2U] = 255U;
    }
    error.clear();
    return true;
}

}  // namespace phonecast::core
