#pragma once

#include "phonecast/core/input/IRemoteInputSender.h"
#include "phonecast/core/protocol/StreamProtocol.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace phonecast::platform::network {

struct VideoServerStats {
    std::uint64_t receivedFrames{};
    std::uint64_t receivedBytes{};
    std::uint64_t keyFrames{};
    std::uint64_t droppedFrames{};
    std::uint64_t resyncRequests{};
    std::size_t queueDepth{};
};

class TcpVideoServer final : public core::IRemoteInputSender {
public:
    TcpVideoServer(std::string pairCode, std::uint16_t port, bool audioEnabled = false);
    ~TcpVideoServer();
    TcpVideoServer(const TcpVideoServer&) = delete;
    TcpVideoServer& operator=(const TcpVideoServer&) = delete;

    bool Start(std::string& error);
    bool Pop(core::protocol::Message& message,
             std::chrono::microseconds* queueAge = nullptr);
    bool PopAudio(core::protocol::Message& message);
    std::uint64_t AudioDropped() const noexcept;
    std::uint64_t AudioReceivedBytes() const noexcept;
    std::uint64_t SessionGeneration() const noexcept;
    bool Send(const core::PointerEvent& event, std::string& error) override;
    bool SendNotificationOpen(std::uint64_t actionToken, std::string& error);
    // Drop queued prediction frames and ask the authenticated sender for a new
    // key frame without closing the stream or remote-input connection.
    bool RequestVideoResync(std::string& error);
    void Stop() noexcept;

    [[nodiscard]] bool Connected() const noexcept;
    [[nodiscard]] std::string Status() const;
    [[nodiscard]] bool RemoteControlStatusKnown() const noexcept;
    [[nodiscard]] core::protocol::RemoteControlStatus RemoteControlStatus() const noexcept;
    [[nodiscard]] VideoServerStats Stats() const noexcept;
    [[nodiscard]] std::uint64_t DroppedMessages() const noexcept;

private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};

}  // namespace phonecast::platform::network
