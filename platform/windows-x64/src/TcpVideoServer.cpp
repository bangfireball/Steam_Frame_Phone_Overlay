#include "phonecast/platform/windows/TcpVideoServer.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <array>
#include <atomic>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace phonecast::platform::windows {
namespace {
using core::protocol::Message;
using core::protocol::MessageType;

bool ReceiveExact(SOCKET socket, std::uint8_t* target, std::size_t size) {
    std::size_t received = 0;
    while (received < size) {
        const int amount = recv(socket, reinterpret_cast<char*>(target + received),
                                static_cast<int>(size - received), 0);
        if (amount <= 0) return false;
        received += static_cast<std::size_t>(amount);
    }
    return true;
}

bool SendExact(SOCKET socket, const std::vector<std::uint8_t>& bytes) {
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const int amount = send(socket, reinterpret_cast<const char*>(bytes.data() + sent),
                                static_cast<int>(bytes.size() - sent), 0);
        if (amount <= 0) return false;
        sent += static_cast<std::size_t>(amount);
    }
    return true;
}
}  // namespace

struct TcpVideoServer::Implementation {
    Implementation(std::string code, std::uint16_t listenPort)
        : pairCode(std::move(code)), port(listenPort) {}

    void SetStatus(std::string value) {
        std::lock_guard<std::mutex> lock(statusMutex);
        status = std::move(value);
        std::cout << "[network] " << status << std::endl;
    }

    void Push(Message message) {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (message.type == MessageType::VideoConfig) {
            messages.clear();
        } else if (message.type == MessageType::VideoFrame && messages.size() >= 4) {
            auto found = messages.begin();
            while (found != messages.end() && found->type != MessageType::VideoFrame) ++found;
            if (found != messages.end()) {
                messages.erase(found);
                ++dropped;
            }
        }
        messages.push_back(std::move(message));
    }

    void HandleClient(SOCKET socket) {
        bool authenticated = false;
        std::array<std::uint8_t, core::protocol::kHeaderSize> header{};
        while (running.load() && ReceiveExact(socket, header.data(), header.size())) {
            Message message;
            std::uint32_t payloadSize = 0;
            std::string error;
            if (!core::protocol::ParseHeader(header.data(), header.size(), message,
                                              payloadSize, error)) {
                SetStatus(error);
                return;
            }
            message.payload.resize(payloadSize);
            if (payloadSize > 0 && !ReceiveExact(socket, message.payload.data(), payloadSize)) return;
            if (!authenticated) {
                const std::string supplied(message.payload.begin(), message.payload.end());
                if (message.type != MessageType::Hello || supplied != pairCode) {
                    SetStatus("Rejected a client with an invalid pairing code");
                    return;
                }
                authenticated = true;
                connected.store(true);
                SetStatus("Phone connected and authenticated");
                continue;
            }
            if (message.type == MessageType::EndStream) return;
            if (message.type == MessageType::Ping) {
                Message pong;
                pong.type = MessageType::Pong;
                pong.timestampMicros = message.timestampMicros;
                if (!SendExact(socket, core::protocol::Serialize(pong))) return;
            } else if (message.type == MessageType::VideoConfig ||
                       message.type == MessageType::VideoFrame) {
                Push(std::move(message));
            }
        }
    }

    void Run() {
        SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        listeningSocket.store(server);
        if (server == INVALID_SOCKET) {
            SetStatus("Could not create listening socket");
            return;
        }
        BOOL exclusive = TRUE;
        setsockopt(server, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);
        if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
            listen(server, 1) == SOCKET_ERROR) {
            SetStatus("Could not listen on TCP port " + std::to_string(port));
            closesocket(server);
            listeningSocket.store(INVALID_SOCKET);
            return;
        }
        SetStatus("Waiting for phone on TCP port " + std::to_string(port));
        while (running.load()) {
            SOCKET accepted = accept(server, nullptr, nullptr);
            if (accepted == INVALID_SOCKET) break;
            clientSocket.store(accepted);
            SetStatus("TCP client connected; waiting for pairing handshake");
            BOOL noDelay = TRUE;
            setsockopt(accepted, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
            HandleClient(accepted);
            connected.store(false);
            closesocket(accepted);
            clientSocket.store(INVALID_SOCKET);
            if (running.load()) SetStatus("Phone disconnected; waiting for reconnection");
        }
    }

    std::string pairCode;
    std::uint16_t port;
    std::atomic_bool running{};
    std::atomic_bool connected{};
    std::atomic<SOCKET> listeningSocket{INVALID_SOCKET};
    std::atomic<SOCKET> clientSocket{INVALID_SOCKET};
    std::thread thread;
    mutable std::mutex statusMutex;
    std::string status;
    mutable std::mutex queueMutex;
    std::deque<Message> messages;
    std::uint64_t dropped{};
    bool winsockStarted{};
};

TcpVideoServer::TcpVideoServer(std::string pairCode, std::uint16_t port)
    : implementation_(std::make_unique<Implementation>(std::move(pairCode), port)) {}
TcpVideoServer::~TcpVideoServer() { Stop(); }

bool TcpVideoServer::Start(std::string& error) {
    auto& state = *implementation_;
    if (state.running.load()) {
        error = "TCP video server is already running.";
        return false;
    }
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        error = "WSAStartup failed.";
        return false;
    }
    state.winsockStarted = true;
    state.running.store(true);
    state.thread = std::thread(&Implementation::Run, &state);
    error.clear();
    return true;
}

bool TcpVideoServer::Pop(Message& message) {
    auto& state = *implementation_;
    std::lock_guard<std::mutex> lock(state.queueMutex);
    if (state.messages.empty()) return false;
    message = std::move(state.messages.front());
    state.messages.pop_front();
    return true;
}

void TcpVideoServer::Stop() noexcept {
    if (!implementation_) return;
    auto& state = *implementation_;
    state.running.store(false);
    SOCKET listening = state.listeningSocket.exchange(INVALID_SOCKET);
    if (listening != INVALID_SOCKET) closesocket(listening);
    SOCKET client = state.clientSocket.exchange(INVALID_SOCKET);
    if (client != INVALID_SOCKET) closesocket(client);
    if (state.thread.joinable()) state.thread.join();
    if (state.winsockStarted) {
        WSACleanup();
        state.winsockStarted = false;
    }
}

bool TcpVideoServer::Connected() const noexcept { return implementation_->connected.load(); }
std::string TcpVideoServer::Status() const {
    std::lock_guard<std::mutex> lock(implementation_->statusMutex);
    return implementation_->status;
}
std::uint64_t TcpVideoServer::DroppedMessages() const noexcept {
    std::lock_guard<std::mutex> lock(implementation_->queueMutex);
    return implementation_->dropped;
}

}  // namespace phonecast::platform::windows
