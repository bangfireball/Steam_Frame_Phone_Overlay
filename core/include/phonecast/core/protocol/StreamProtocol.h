#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace phonecast::core::protocol {

constexpr std::uint8_t kProtocolVersion = 1;
constexpr std::size_t kHeaderSize = 32;
constexpr std::uint32_t kMaximumPayloadSize = 4U * 1024U * 1024U;

enum class MessageType : std::uint8_t {
    Hello = 1,
    VideoConfig = 2,
    VideoFrame = 3,
    Ping = 4,
    Pong = 5,
    EndStream = 6,
    RequestKeyFrame = 7,
};

enum MessageFlags : std::uint16_t {
    None = 0,
    KeyFrame = 1U << 0U,
};

struct Message {
    MessageType type{MessageType::Hello};
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint64_t timestampMicros{};
    std::uint16_t width{};
    std::uint16_t height{};
    std::vector<std::uint8_t> payload;
};

std::vector<std::uint8_t> Serialize(const Message& message);
bool ParseHeader(const std::uint8_t* data, std::size_t size, Message& message,
                 std::uint32_t& payloadSize, std::string& error);
bool IsValidPairCode(const std::string& code) noexcept;

}  // namespace phonecast::core::protocol
