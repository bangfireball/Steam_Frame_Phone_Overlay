#pragma once

#include "phonecast/core/protocol/StreamProtocol.h"
#include <cstdint>
#include <vector>

namespace phonecast::core::audio {
constexpr std::uint32_t SampleRate = 48000;
constexpr std::uint32_t Channels = 2;
constexpr std::uint32_t FramesPerBlock = 480;
constexpr std::size_t BytesPerBlock = FramesPerBlock * Channels * 2;
constexpr std::size_t MaximumFramePayload = 12 + BytesPerBlock;

struct Block {
    std::uint64_t epoch{};
    std::uint64_t sequence{};
    std::uint64_t timestampMicros{}; // First sample frame, Android monotonic clock.
    std::vector<std::uint8_t> samples; // Interleaved signed PCM16 little endian.
};
enum class CaptureStatus : std::uint8_t { Off, Active, Silent, Error };
std::vector<std::uint8_t> Capability();
bool HasCapability(const std::vector<std::uint8_t>& payload);
std::vector<std::uint8_t> Configuration(std::uint64_t epoch);
bool ParseConfiguration(const protocol::Message& message, std::uint64_t& epoch);
std::vector<std::uint8_t> EncodeBlock(const Block& block);
bool ParseBlock(const protocol::Message& message, Block& block);
std::vector<std::uint8_t> Status(std::uint64_t epoch, CaptureStatus status);
bool ParseStatus(const protocol::Message& message, std::uint64_t& epoch, CaptureStatus& status);
bool ValidEnvelope(const protocol::Message& message);
} // namespace phonecast::core::audio
