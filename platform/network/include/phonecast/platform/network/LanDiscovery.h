#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace phonecast::platform::network {
// Bounded public LAN hints only; never carries pairing or stream data.
std::string DiscoveryReply(const std::string& query, std::uint16_t port,
                           const std::string& name, const std::string& version);
class LanDiscovery final {
public:
    LanDiscovery();
    ~LanDiscovery();
    bool Start(std::uint16_t streamPort);
    void Stop();
private:
    struct Implementation;
    std::unique_ptr<Implementation> impl_;
};
}
