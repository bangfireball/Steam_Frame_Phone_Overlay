#include "phonecast/platform/network/TcpVideoServer.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

using SOCKET = int;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
using BOOL = int;
inline int closesocket(SOCKET socket) { return close(socket); }
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace phonecast::platform::network {
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
                                static_cast<int>(bytes.size() - sent),
#ifdef _WIN32
                                0
#else
                                MSG_NOSIGNAL
#endif
        );
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

    struct QueuedMessage {
        Message message;
        std::chrono::steady_clock::time_point receivedAt;
    };

    std::uint64_t DiscardQueuedFrames() {
        const auto originalSize = messages.size();
        messages.erase(std::remove_if(messages.begin(), messages.end(),
            [](const QueuedMessage& queued) {
                return queued.message.type == MessageType::VideoFrame;
            }), messages.end());
        return static_cast<std::uint64_t>(originalSize - messages.size());
    }

    bool Push(Message message) {
        std::lock_guard<std::mutex> lock(queueMutex);
        const auto now = std::chrono::steady_clock::now();
        if (message.type == MessageType::VideoConfig) {
            dropped += DiscardQueuedFrames();
            messages.clear();
            waitingForKeyFrame = true;
            haveExpectedSequence = false;
            keyFrameRequestPending = true;
            ++resyncRequests;
            messages.push_back({std::move(message), now});
            return true;
        }
        if (message.type == MessageType::Notification) {
            // Notification cards must never disturb video prediction state. If
            // the queue is busy, discard an older card rather than a video AU.
            if (messages.size() >= kMaximumQueuedMessages) {
                const auto old = std::find_if(messages.begin(), messages.end(),
                    [](const QueuedMessage& queued) {
                        return queued.message.type == MessageType::Notification;
                    });
                if (old == messages.end()) return false;
                messages.erase(old);
            }
            messages.push_back({std::move(message), now});
            return false;
        }

        ++receivedFrames;
        receivedBytes += message.payload.size();
        const bool keyFrame = (message.flags & core::protocol::MessageFlags::KeyFrame) != 0;
        if (keyFrame) {
            ++keyFrames;
            if (haveExpectedSequence && message.sequence > expectedSequence) {
                dropped += message.sequence - expectedSequence;
            }
            waitingForKeyFrame = false;
            keyFrameRequestPending = false;
            expectedSequence = message.sequence + 1;
            haveExpectedSequence = true;
        } else if (waitingForKeyFrame) {
            ++dropped;
            return false;
        } else if (haveExpectedSequence && message.sequence != expectedSequence) {
            dropped += DiscardQueuedFrames() + 1;
            waitingForKeyFrame = true;
            haveExpectedSequence = false;
            if (!keyFrameRequestPending) {
                keyFrameRequestPending = true;
                ++resyncRequests;
                return true;
            }
            return false;
        } else {
            expectedSequence = message.sequence + 1;
            haveExpectedSequence = true;
        }

        if (messages.size() >= kMaximumQueuedMessages) {
            dropped += DiscardQueuedFrames() + 1;
            waitingForKeyFrame = true;
            haveExpectedSequence = false;
            if (!keyFrameRequestPending) {
                keyFrameRequestPending = true;
                ++resyncRequests;
                return true;
            }
            return false;
        }
        messages.push_back({std::move(message), now});
        return false;
    }

    bool SendMessage(SOCKET socket, const Message& message) {
        std::lock_guard<std::mutex> lock(sendMutex);
        return SendExact(socket, core::protocol::Serialize(message));
    }

    bool SendKeyFrameRequest(SOCKET socket) {
        Message request;
        request.type = MessageType::RequestKeyFrame;
        return SendMessage(socket, request);
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
            if (message.type == MessageType::RemoteControlStatus) {
                core::protocol::RemoteControlStatus remoteStatus;
                if (!core::protocol::ParseRemoteControlStatus(
                        message.payload.data(), message.payload.size(), remoteStatus, error)) {
                    SetStatus("Rejected malformed remote-control gate status");
                    continue;
                }
                remoteControlAppEnabled.store(remoteStatus.appEnabled);
                remoteControlAccessibilityEnabled.store(remoteStatus.accessibilityEnabled);
                remoteControlStatusKnown.store(true);
                if (remoteStatus.Ready())
                    SetStatus("Phone connected; remote control ready");
                else if (!remoteStatus.appEnabled && !remoteStatus.accessibilityEnabled)
                    SetStatus("Warning: both Android remote-control gates are disabled");
                else if (!remoteStatus.appEnabled)
                    SetStatus("Warning: Android app remote-control consent is disabled");
                else
                    SetStatus("Warning: PhoneCast Accessibility service is disabled");
                continue;
            }
            if (message.type == MessageType::Ping) {
                Message pong;
                pong.type = MessageType::Pong;
                pong.timestampMicros = message.timestampMicros;
                if (!SendMessage(socket, pong)) return;
            } else if (message.type == MessageType::VideoConfig ||
                       message.type == MessageType::VideoFrame ||
                       message.type == MessageType::Notification) {
                if (Push(std::move(message)) && !SendKeyFrameRequest(socket)) return;
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
#ifdef _WIN32
        BOOL exclusive = TRUE;
        setsockopt(server, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
#else
        int reuseAddress = 1;
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress));
#endif
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
            int noDelay = 1;
            setsockopt(accepted, IPPROTO_TCP, TCP_NODELAY,
#ifdef _WIN32
                       reinterpret_cast<const char*>(&noDelay),
#else
                       &noDelay,
#endif
                       sizeof(noDelay));
            remoteControlStatusKnown.store(false);
            remoteControlAppEnabled.store(false);
            remoteControlAccessibilityEnabled.store(false);
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                messages.clear();
                waitingForKeyFrame = true;
                haveExpectedSequence = false;
                keyFrameRequestPending = false;
            }
            HandleClient(accepted);
            connected.store(false);
            remoteControlStatusKnown.store(false);
            remoteControlAppEnabled.store(false);
            remoteControlAccessibilityEnabled.store(false);
            SOCKET expectedClient = accepted;
            if (clientSocket.compare_exchange_strong(expectedClient, INVALID_SOCKET))
                closesocket(accepted);
            if (running.load()) SetStatus("Phone disconnected; waiting for reconnection");
        }
        SOCKET expectedServer = server;
        if (listeningSocket.compare_exchange_strong(expectedServer, INVALID_SOCKET))
            closesocket(server);
    }

    std::string pairCode;
    std::uint16_t port;
    std::atomic_bool running{};
    std::atomic_bool connected{};
    std::atomic_bool remoteControlStatusKnown{};
    std::atomic_bool remoteControlAppEnabled{};
    std::atomic_bool remoteControlAccessibilityEnabled{};
    std::atomic<SOCKET> listeningSocket{INVALID_SOCKET};
    std::atomic<SOCKET> clientSocket{INVALID_SOCKET};
    std::thread thread;
    mutable std::mutex statusMutex;
    std::string status;
    std::mutex sendMutex;
    // Absorb startup and short TCP/Android scheduler bursts without repeatedly
    // throwing away an otherwise decodable prediction chain. The decoder is
    // faster than the target 30 FPS and drains this queue after initialization.
    static constexpr std::size_t kMaximumQueuedMessages = 16;
    mutable std::mutex queueMutex;
    std::deque<QueuedMessage> messages;
    std::uint64_t receivedFrames{};
    std::uint64_t receivedBytes{};
    std::uint64_t keyFrames{};
    std::uint64_t dropped{};
    std::uint64_t resyncRequests{};
    std::uint64_t expectedSequence{};
    bool waitingForKeyFrame{true};
    bool haveExpectedSequence{};
    bool keyFrameRequestPending{};
#ifdef _WIN32
    bool winsockStarted{};
#endif
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
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        error = "WSAStartup failed.";
        return false;
    }
    state.winsockStarted = true;
#endif
    state.running.store(true);
    state.thread = std::thread(&Implementation::Run, &state);
    error.clear();
    return true;
}

bool TcpVideoServer::Pop(Message& message, std::chrono::microseconds* queueAge) {
    auto& state = *implementation_;
    std::lock_guard<std::mutex> lock(state.queueMutex);
    if (state.messages.empty()) return false;
    auto queued = std::move(state.messages.front());
    state.messages.pop_front();
    message = std::move(queued.message);
    if (queueAge != nullptr) {
        *queueAge = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - queued.receivedAt);
    }
    return true;
}

void TcpVideoServer::Stop() noexcept {
    if (!implementation_) return;
    auto& state = *implementation_;
    state.running.store(false);
    SOCKET listening = state.listeningSocket.exchange(INVALID_SOCKET);
    if (listening != INVALID_SOCKET) {
#ifdef _WIN32
        shutdown(listening, SD_BOTH);
#else
        shutdown(listening, SHUT_RDWR);
#endif
        closesocket(listening);
    }
    SOCKET client = state.clientSocket.exchange(INVALID_SOCKET);
    if (client != INVALID_SOCKET) {
#ifdef _WIN32
        shutdown(client, SD_BOTH);
#else
        shutdown(client, SHUT_RDWR);
#endif
        closesocket(client);
    }
    if (state.thread.joinable()) state.thread.join();
#ifdef _WIN32
    if (state.winsockStarted) {
        WSACleanup();
        state.winsockStarted = false;
    }
#endif
}

bool TcpVideoServer::Send(const core::PointerEvent& event, std::string& error) {
    auto& state = *implementation_;
    if (!state.connected.load()) {
        error = "The phone is not connected; remote input was not sent.";
        return false;
    }
    const SOCKET socket = state.clientSocket.load();
    if (socket == INVALID_SOCKET) {
        error = "The phone connection closed before remote input could be sent.";
        return false;
    }
    Message message;
    message.type = MessageType::RemoteInput;
    message.sequence = event.sequence;
    message.payload = core::protocol::SerializePointerEvent(event);
    if (!state.SendMessage(socket, message)) {
        error = "Sending remote input to the phone failed.";
        return false;
    }
    error.clear();
    return true;
}

bool TcpVideoServer::SendNotificationOpen(std::uint64_t actionToken, std::string& error) {
    auto& state = *implementation_;
    if (actionToken == 0U) {
        error.clear();
        return true;
    }
    if (!state.connected.load()) {
        error = "The phone is not connected; the notification could not be opened.";
        return false;
    }
    const SOCKET socket = state.clientSocket.load();
    if (socket == INVALID_SOCKET) {
        error = "The phone connection closed before the notification could be opened.";
        return false;
    }
    Message message;
    message.type = MessageType::NotificationOpen;
    message.sequence = actionToken;
    if (!state.SendMessage(socket, message)) {
        error = "Sending the notification open action to the phone failed.";
        return false;
    }
    error.clear();
    return true;
}

bool TcpVideoServer::Connected() const noexcept { return implementation_->connected.load(); }
std::string TcpVideoServer::Status() const {
    std::lock_guard<std::mutex> lock(implementation_->statusMutex);
    return implementation_->status;
}
bool TcpVideoServer::RemoteControlStatusKnown() const noexcept {
    return implementation_->remoteControlStatusKnown.load();
}
core::protocol::RemoteControlStatus TcpVideoServer::RemoteControlStatus() const noexcept {
    return {implementation_->remoteControlAppEnabled.load(),
            implementation_->remoteControlAccessibilityEnabled.load()};
}
VideoServerStats TcpVideoServer::Stats() const noexcept {
    std::lock_guard<std::mutex> lock(implementation_->queueMutex);
    return {implementation_->receivedFrames, implementation_->receivedBytes,
            implementation_->keyFrames, implementation_->dropped,
            implementation_->resyncRequests, implementation_->messages.size()};
}
std::uint64_t TcpVideoServer::DroppedMessages() const noexcept {
    return Stats().droppedFrames;
}

}  // namespace phonecast::platform::network
