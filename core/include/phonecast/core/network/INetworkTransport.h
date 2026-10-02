#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace phonecast::core {

class INetworkTransport {
public:
    virtual ~INetworkTransport() = default;
    virtual bool Start(std::string& error) = 0;
    virtual bool Receive(std::vector<std::uint8_t>& payload, std::string& error) = 0;
    virtual void Stop() noexcept = 0;
};

}  // namespace phonecast::core
