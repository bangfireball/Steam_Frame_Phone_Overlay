#include "phonecast/core/protocol/StreamProtocol.h"

#include <algorithm>
#include <array>
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
           type <= static_cast<std::uint8_t>(MessageType::AudioStatus);
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

constexpr std::size_t kNotificationV1HeaderSize = 18;
constexpr std::size_t kNotificationV2HeaderSize = 26;
constexpr std::size_t kMaximumNotificationFieldSize = 1024;

bool NotificationFieldValid(const std::string& field) {
    return field.size() <= kMaximumNotificationFieldSize && field.size() <= 0xffffU;
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
    const auto type = static_cast<MessageType>(data[5]);
    if ((type == MessageType::AudioConfig && payloadSize != 16) ||
        (type == MessageType::AudioFrame && payloadSize != 1932) ||
        (type == MessageType::AudioStatus && payloadSize != 12)) {
        error = "Invalid bounded audio payload size.";
        return false;
    }
    message.type = type;
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

std::vector<std::uint8_t> SerializeNotification(const NotificationEvent& notification) {
    if (!NotificationFieldValid(notification.applicationName) ||
        !NotificationFieldValid(notification.title) ||
        !NotificationFieldValid(notification.body) ||
        !NotificationFieldValid(notification.packageName)) return {};
    const std::size_t payloadSize = kNotificationV2HeaderSize +
        notification.applicationName.size() + notification.title.size() +
        notification.body.size() + notification.packageName.size();
    if (payloadSize > kMaximumPayloadSize) return {};
    std::vector<std::uint8_t> bytes(payloadSize, 0);
    bytes[0] = 2;
    bytes[1] = notification.contentRedacted ? 1U : 0U;
    Write16(bytes, 2, static_cast<std::uint16_t>(notification.applicationName.size()));
    Write16(bytes, 4, static_cast<std::uint16_t>(notification.title.size()));
    Write16(bytes, 6, static_cast<std::uint16_t>(notification.body.size()));
    Write16(bytes, 8, static_cast<std::uint16_t>(notification.packageName.size()));
    Write64(bytes, 10, notification.postedAtMillis);
    Write64(bytes, 18, notification.actionToken);
    auto output = bytes.begin() + static_cast<std::ptrdiff_t>(kNotificationV2HeaderSize);
    for (const auto* field : {&notification.applicationName, &notification.title,
                              &notification.body, &notification.packageName}) {
        output = std::copy(field->begin(), field->end(), output);
    }
    return bytes;
}

std::vector<std::uint8_t> SerializeRemoteControlStatus(const RemoteControlStatus& status) {
    return {1U, static_cast<std::uint8_t>((status.appEnabled ? 1U : 0U) |
                                         (status.accessibilityEnabled ? 2U : 0U))};
}

bool ParseRemoteControlStatus(const std::uint8_t* data, std::size_t size,
                              RemoteControlStatus& status, std::string& error) {
    if (data == nullptr || size != 2U) {
        error = "Remote-control status payload must be exactly 2 bytes.";
        return false;
    }
    if (data[0] != 1U || (data[1] & 0xfcU) != 0U) {
        error = "Unsupported remote-control status version or flags.";
        return false;
    }
    status.appEnabled = (data[1] & 1U) != 0U;
    status.accessibilityEnabled = (data[1] & 2U) != 0U;
    error.clear();
    return true;
}

bool ParseNotification(const std::uint8_t* data, std::size_t size,
                       NotificationEvent& notification, std::string& error) {
    if (data == nullptr || size < kNotificationV1HeaderSize) {
        error = "Notification payload is incomplete.";
        return false;
    }
    if ((data[0] != 1U && data[0] != 2U) || (data[1] & 0xfeU) != 0U) {
        error = "Unsupported notification payload version or flags.";
        return false;
    }
    const std::size_t headerSize = data[0] == 2U
        ? kNotificationV2HeaderSize : kNotificationV1HeaderSize;
    if (size < headerSize) {
        error = "Notification payload is incomplete.";
        return false;
    }
    const std::array<std::size_t, 4> lengths{
        Read16(data + 2), Read16(data + 4), Read16(data + 6), Read16(data + 8)};
    std::size_t expected = headerSize;
    for (const auto length : lengths) {
        if (length > kMaximumNotificationFieldSize || expected > size || length > size - expected) {
            error = "Notification field length is invalid.";
            return false;
        }
        expected += length;
    }
    if (expected != size) {
        error = "Notification payload has trailing or missing data.";
        return false;
    }
    std::size_t offset = headerSize;
    auto readString = [&](std::size_t length) {
        std::string value(reinterpret_cast<const char*>(data + offset), length);
        offset += length;
        return value;
    };
    notification.applicationName = readString(lengths[0]);
    notification.title = readString(lengths[1]);
    notification.body = readString(lengths[2]);
    notification.packageName = readString(lengths[3]);
    notification.postedAtMillis = Read64(data + 10);
    notification.actionToken = data[0] == 2U ? Read64(data + 18) : 0U;
    notification.contentRedacted = (data[1] & 1U) != 0U;
    if (notification.applicationName.empty() || notification.packageName.empty()) {
        error = "Notification source is missing.";
        return false;
    }
    error.clear();
    return true;
}

}  // namespace phonecast::core::protocol
