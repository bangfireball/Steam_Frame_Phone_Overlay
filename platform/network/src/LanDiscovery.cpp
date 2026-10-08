#include "phonecast/platform/network/LanDiscovery.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
using SOCKET = int;
constexpr SOCKET INVALID_SOCKET = -1;
inline int closesocket(SOCKET value) { return close(value); }
#endif
#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <algorithm>

namespace phonecast::platform::network {
std::string DiscoveryReply(const std::string& query, std::uint16_t port,
                           const std::string& name, const std::string& version) {
    const std::string prefix = "PCVR_DISCOVER 1 ";
    if (query.size() != prefix.size() + 32 || query.compare(0, prefix.size(), prefix) != 0 || port == 0) return {};
    const auto nonce = query.substr(prefix.size());
    if (nonce.find_first_not_of("0123456789abcdef") != std::string::npos) return {};
    std::string safeName = name.substr(0, 48);
    for (auto& c : safeName) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_' || c == ' ')) c = '-';
    if (safeName.empty()) safeName = "PhoneCast headset";
    if (version.empty() || version.size() > 16 || version.find_first_not_of("0123456789.") != std::string::npos) return {};
    return "PCVR_RECEIVER 1 " + nonce + " " + std::to_string(port) + " " + version + " " + safeName;
}
struct LanDiscovery::Implementation {
    SOCKET socket{INVALID_SOCKET};
    std::atomic<bool> running{false};
    std::thread worker;
#ifdef _WIN32
    bool winsock{false};
#endif
};
LanDiscovery::LanDiscovery() : impl_(std::make_unique<Implementation>()) {}
LanDiscovery::~LanDiscovery() { Stop(); }
bool LanDiscovery::Start(std::uint16_t streamPort) {
    Stop();
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
    impl_->winsock = true;
#endif
    impl_->socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (impl_->socket == INVALID_SOCKET) { Stop(); return false; }
    sockaddr_in address{};
    address.sin_family = AF_INET; address.sin_port = htons(49322); address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(impl_->socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) { Stop(); return false; }
    std::array<char, 256> host{};
    std::string name = "PhoneCast headset";
    if (gethostname(host.data(), static_cast<int>(host.size() - 1)) == 0) name = host.data();
    impl_->running = true;
    impl_->worker = std::thread([this, streamPort, name] {
        auto lastReply = std::chrono::steady_clock::time_point{};
        while (impl_->running) {
            fd_set set; FD_ZERO(&set); FD_SET(impl_->socket, &set);
            timeval wait{0, 200000};
            if (select(static_cast<int>(impl_->socket + 1), &set, nullptr, nullptr, &wait) <= 0) continue;
            std::array<char, 128> bytes{}; sockaddr_in sender{};
#ifdef _WIN32
            int size = sizeof(sender);
#else
            socklen_t size = sizeof(sender);
#endif
            const int count = recvfrom(impl_->socket, bytes.data(), static_cast<int>(bytes.size()), 0,
                                      reinterpret_cast<sockaddr*>(&sender), &size);
            if (count <= 0) continue;
            const auto ip = ntohl(sender.sin_addr.s_addr);
            const bool local = (ip >> 24) == 10 || (ip >> 24) == 127 ||
                (ip >> 16) == 0xc0a8 || (ip >> 20) == 0xac1 || (ip >> 16) == 0xa9fe;
            if (!local) continue; // IPv4 private/link-local LAN only; manual TCP remains available.
            const auto now = std::chrono::steady_clock::now();
            if (now - lastReply < std::chrono::milliseconds(50)) continue; // global response cap, no amplification flood
            const auto reply = DiscoveryReply(std::string(bytes.data(), static_cast<std::size_t>(count)), streamPort, name, PHONECAST_VERSION);
            if (reply.empty()) continue;
            sendto(impl_->socket, reply.data(), static_cast<int>(reply.size()), 0,
                   reinterpret_cast<sockaddr*>(&sender), size);
            lastReply = now;
        }
    });
    return true;
}
void LanDiscovery::Stop() {
    impl_->running = false;
    if (impl_->worker.joinable()) impl_->worker.join();
    if (impl_->socket != INVALID_SOCKET) { closesocket(impl_->socket); impl_->socket = INVALID_SOCKET; }
#ifdef _WIN32
    if (impl_->winsock) { WSACleanup(); impl_->winsock = false; }
#endif
}
}
