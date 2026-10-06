#include "phonecast/core/audio/AudioProtocol.h"
#include <algorithm>
#include <limits>
#include <utility>

namespace phonecast::core::audio {
namespace {
void Write64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> ((7 - i) * 8));
}
std::uint64_t Read64(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) value = (value << 8) | bytes[offset + i];
    return value;
}
}
std::vector<std::uint8_t> Capability() { return {'P','C','A','C',1,1,0,0}; }
bool HasCapability(const std::vector<std::uint8_t>& payload) { return payload == Capability(); }
bool ValidEnvelope(const protocol::Message& message) {
    return message.flags == 0 && message.width == 0 && message.height == 0;
}
std::vector<std::uint8_t> Configuration(std::uint64_t epoch) {
    if (epoch == 0) return {};
    std::vector<std::uint8_t> bytes{1,1,2,0,0,0,0xbb,0x80,0,0,0,0,0,0,0,0};
    Write64(bytes,8,epoch);
    return bytes;
}
bool ParseConfiguration(const protocol::Message& message, std::uint64_t& epoch) {
    if (message.type != protocol::MessageType::AudioConfig || !ValidEnvelope(message) ||
        message.payload.size() != 16) return false;
    const auto& p = message.payload;
    if (!std::equal(p.begin(),p.begin()+8, Configuration(1).begin())) return false;
    const auto value = Read64(p,8);
    if (value == 0) return false;
    epoch = value;
    return true;
}
std::vector<std::uint8_t> EncodeBlock(const Block& block) {
    if (block.epoch == 0 || block.samples.size() != BytesPerBlock ||
        block.timestampMicros == 0 || block.timestampMicros >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) - 10000) return {};
    std::vector<std::uint8_t> bytes(MaximumFramePayload,0);
    Write64(bytes,0,block.epoch);
    bytes[8] = 1; bytes[9] = 0xe0; // 480 stereo sample frames.
    std::copy(block.samples.begin(),block.samples.end(),bytes.begin()+12);
    return bytes;
}
bool ParseBlock(const protocol::Message& message, Block& block) {
    if (message.type != protocol::MessageType::AudioFrame || !ValidEnvelope(message) ||
        message.payload.size() != MaximumFramePayload || message.timestampMicros == 0 ||
        message.timestampMicros > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) - 10000)
        return false;
    const auto& p = message.payload;
    const auto epoch = Read64(p,0);
    if (epoch == 0 || p[8] != 1 || p[9] != 0xe0 || p[10] != 0 || p[11] != 0) return false;
    Block parsed{epoch,message.sequence,message.timestampMicros,{}};
    parsed.samples.assign(p.begin()+12,p.end());
    block = std::move(parsed);
    return true;
}
std::vector<std::uint8_t> Status(std::uint64_t epoch, CaptureStatus status) {
    if (epoch == 0 || static_cast<unsigned>(status) > 3) return {};
    std::vector<std::uint8_t> bytes(12,0);
    bytes[0] = 1; bytes[1] = static_cast<std::uint8_t>(status);
    Write64(bytes,4,epoch);
    return bytes;
}
bool ParseStatus(const protocol::Message& message, std::uint64_t& epoch, CaptureStatus& status) {
    if (message.type != protocol::MessageType::AudioStatus || !ValidEnvelope(message) ||
        message.payload.size() != 12) return false;
    const auto& p = message.payload;
    const auto value = Read64(p,4);
    if (p[0] != 1 || p[1] > 3 || p[2] != 0 || p[3] != 0 || value == 0) return false;
    epoch = value; status = static_cast<CaptureStatus>(p[1]);
    return true;
}
} // namespace phonecast::core::audio
