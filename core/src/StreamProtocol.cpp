#include "phonecast/core/protocol/StreamProtocol.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace phonecast::core::protocol {
namespace {
constexpr std::uint8_t kMagic[] = {'P', 'C', 'V', 'R'};

void Write16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value >> 8U);
    bytes[offset + 1] = static_cast<std::uint8_t>(value);
}
void Write32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (int index = 3; index >= 0; --index) {
        bytes[offset + static_cast<std::size_t>(3 - index)] =
            static_cast<std::uint8_t>(value >> (index * 8));
    }
}
void Write64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
    for (int index = 7; index >= 0; --index) {
        bytes[offset + static_cast<std::size_t>(7 - index)] =
            static_cast<std::uint8_t>(value >> (index * 8));
    }
}
std::uint16_t Read16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8U) | data[1]);
}
std::uint32_t Read32(const std::uint8_t* data) {
    std::uint32_t value = 0;
    for (int index = 0; index < 4; ++index) value = (value << 8U) | data[index];
    return value;
}
std::uint64_t Read64(const std::uint8_t* data) {
    std::uint64_t value = 0;
    for (int index = 0; index < 8; ++index) value = (value << 8U) | data[index];
    return value;
}
bool IsKnownType(std::uint8_t type) {
    return type >= static_cast<std::uint8_t>(MessageType::Hello) &&
           type <= static_cast<std::uint8_t>(MessageType::RemoteInput);
}

void WriteFloat(std::vector<std::uint8_t>& bytes, std::size_t offset, float value) {
    std::uint32_t bits{};
    static_assert(sizeof(bits) == sizeof(value), "32-bit float required");
    std::memcpy(&bits, &value, sizeof(bits));
    Write32(bytes, offset, bits);
}

float ReadFloat(const std::uint8_t* data) {
    const std::uint32_t bits = Read32(data);
    float value{};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool IsKnownPointerType(std::uint8_t type) {
    return type >= static_cast<std::uint8_t>(phonecast::core::PointerEvent::Type::Down) &&
           type <= static_cast<std::uint8_t>(phonecast::core::PointerEvent::Type::Back);
}
}  // namespace

std::vector<std::uint8_t> Serialize(const Message& message) {
    if (message.payload.size() > kMaximumPayloadSize) return {};
    std::vector<std::uint8_t> bytes(kHeaderSize + message.payload.size());
    std::copy(std::begin(kMagic), std::end(kMagic), bytes.begin());
    bytes[4] = kProtocolVersion;
    bytes[5] = static_cast<std::uint8_t>(message.type);
    Write16(bytes, 6, message.flags);
    Write32(bytes, 8, static_cast<std::uint32_t>(message.payload.size()));
    Write64(bytes, 12, message.sequence);
    Write64(bytes, 20, message.timestampMicros);
    Write16(bytes, 28, message.width);
    Write16(bytes, 30, message.height);
    std::copy(message.payload.begin(), message.payload.end(), bytes.begin() + kHeaderSize);
    return bytes;
}

bool ParseHeader(const std::uint8_t* data, std::size_t size, Message& message,
                 std::uint32_t& payloadSize, std::string& error) {
    if (data == nullptr || size < kHeaderSize) {
        error = "Incomplete stream protocol header.";
        return false;
    }
    if (!std::equal(std::begin(kMagic), std::end(kMagic), data)) {
        error = "Invalid stream protocol magic.";
        return false;
    }
    if (data[4] != kProtocolVersion) {
        error = "Unsupported stream protocol version: " + std::to_string(data[4]) + '.';
        return false;
    }
    if (!IsKnownType(data[5])) {
        error = "Unknown stream message type.";
        return false;
    }
    payloadSize = Read32(data + 8);
    if (payloadSize > kMaximumPayloadSize) {
        error = "Stream payload exceeds the 4 MiB safety limit.";
        return false;
    }
    message.type = static_cast<MessageType>(data[5]);
    message.flags = Read16(data + 6);
    message.sequence = Read64(data + 12);
    message.timestampMicros = Read64(data + 20);
    message.width = Read16(data + 28);
    message.height = Read16(data + 30);
    message.payload.clear();
    error.clear();
    return true;
}

bool IsValidPairCode(const std::string& code) noexcept {
    return code.size() == 6 &&
           std::all_of(code.begin(), code.end(), [](char value) { return value >= '0' && value <= '9'; });
}

std::vector<std::uint8_t> SerializePointerEvent(const phonecast::core::PointerEvent& event) {
    std::vector<std::uint8_t> bytes(16, 0);
    bytes[0] = static_cast<std::uint8_t>(event.type);
    WriteFloat(bytes, 4, event.normalizedX);
    WriteFloat(bytes, 8, event.normalizedY);
    WriteFloat(bytes, 12, event.scrollDelta);
    return bytes;
}

bool ParsePointerEvent(const std::uint8_t* data, std::size_t size,
                       phonecast::core::PointerEvent& event, std::string& error) {
    if (data == nullptr || size != 16) {
        error = "Remote-input payload must be exactly 16 bytes.";
        return false;
    }
    if (!IsKnownPointerType(data[0])) {
        error = "Unknown remote-input event type.";
        return false;
    }
    const float x = ReadFloat(data + 4);
    const float y = ReadFloat(data + 8);
    const float scroll = ReadFloat(data + 12);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(scroll) ||
        x < 0.0F || x > 1.0F || y < 0.0F || y > 1.0F ||
        scroll < -1.0F || scroll > 1.0F) {
        error = "Remote-input coordinates are outside the normalized range.";
        return false;
    }
    event.type = static_cast<phonecast::core::PointerEvent::Type>(data[0]);
    event.normalizedX = x;
    event.normalizedY = y;
    event.scrollDelta = scroll;
    error.clear();
    return true;
}

}  // namespace phonecast::core::protocol
