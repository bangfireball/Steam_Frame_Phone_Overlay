#pragma once

#include "phonecast/core/protocol/StreamProtocol.h"

#include <cstdint>
#include <memory>
#include <string>

namespace phonecast::platform::windows {

class TcpVideoServer {
public:
    TcpVideoServer(std::string pairCode, std::uint16_t port);
    ~TcpVideoServer();
    TcpVideoServer(const TcpVideoServer&) = delete;
    TcpVideoServer& operator=(const TcpVideoServer&) = delete;

    bool Start(std::string& error);
    bool Pop(core::protocol::Message& message);
    void Stop() noexcept;

    [[nodiscard]] bool Connected() const noexcept;
    [[nodiscard]] std::string Status() const;
    [[nodiscard]] std::uint64_t DroppedMessages() const noexcept;

private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};

}  // namespace phonecast::platform::windows
