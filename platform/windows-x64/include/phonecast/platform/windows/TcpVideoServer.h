#pragma once

#include "phonecast/core/input/IRemoteInputSender.h"
#include "phonecast/core/protocol/StreamProtocol.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace phonecast::platform::windows {

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
    TcpVideoServer(std::string pairCode, std::uint16_t port);
    ~TcpVideoServer();
    TcpVideoServer(const TcpVideoServer&) = delete;
    TcpVideoServer& operator=(const TcpVideoServer&) = delete;

    bool Start(std::string& error);
    bool Pop(core::protocol::Message& message,
             std::chrono::microseconds* queueAge = nullptr);
    bool Send(const core::PointerEvent& event, std::string& error) override;
    void Stop() noexcept;

    [[nodiscard]] bool Connected() const noexcept;
    [[nodiscard]] std::string Status() const;
    [[nodiscard]] VideoServerStats Stats() const noexcept;
    [[nodiscard]] std::uint64_t DroppedMessages() const noexcept;

private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};

}  // namespace phonecast::platform::windows
