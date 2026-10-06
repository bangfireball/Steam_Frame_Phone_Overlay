#pragma once

#include <cstdint>
#include <vector>

namespace phonecast::core {

enum class PixelFormat { Rgba8 };

struct VideoFrame {
    std::uint32_t width{};
    std::uint32_t height{};
    PixelFormat format{PixelFormat::Rgba8};
    std::uint64_t sequence{};
    // Actual decoded picture PTS; zero means unavailable/generated content.
    std::uint64_t timestampMicros{};
    std::vector<std::uint8_t> pixels;

    [[nodiscard]] bool IsValid() const noexcept {
        return width > 0 && height > 0 && format == PixelFormat::Rgba8 &&
               pixels.size() == static_cast<std::size_t>(width) * height * 4U;
    }
};

}  // namespace phonecast::core
