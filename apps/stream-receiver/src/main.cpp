#include "phonecast/core/protocol/StreamProtocol.h"
#include "phonecast/core/streaming/VideoFrame.h"
#include "phonecast/platform/windows/DesktopPreview.h"
#include "phonecast/platform/windows/MfH264Decoder.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using phonecast::core::protocol::Message;
using phonecast::core::protocol::MessageType;

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

class MessageQueue {
public:
    void Push(Message message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (message.type == MessageType::VideoConfig) {
            messages_.clear();
        } else if (message.type == MessageType::VideoFrame && messages_.size() >= 4) {
            auto found = messages_.begin();
            while (found != messages_.end() && found->type != MessageType::VideoFrame) ++found;
            if (found != messages_.end()) {
                messages_.erase(found);
                ++dropped_;
            }
        }
        messages_.push_back(std::move(message));
    }

    bool Pop(Message& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (messages_.empty()) return false;
        message = std::move(messages_.front());
        messages_.pop_front();
        return true;
    }

    std::uint64_t Dropped() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return dropped_;
    }

private:
    mutable std::mutex mutex_;
    std::deque<Message> messages_;
    std::uint64_t dropped_{};
};

class TcpServer {
public:
    TcpServer(MessageQueue& queue, std::string pairCode, std::uint16_t port)
        : queue_(queue), pairCode_(std::move(pairCode)), port_(port) {}

    bool Start(std::string& error) {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            error = "WSAStartup failed.";
            return false;
        }
        running_.store(true);
        thread_ = std::thread(&TcpServer::Run, this);
        return true;
    }

    void Stop() {
        running_.store(false);
        SOCKET listening = listeningSocket_.exchange(INVALID_SOCKET);
        if (listening != INVALID_SOCKET) closesocket(listening);
        SOCKET client = clientSocket_.exchange(INVALID_SOCKET);
        if (client != INVALID_SOCKET) closesocket(client);
        if (thread_.joinable()) thread_.join();
        WSACleanup();
    }

    bool Connected() const { return connected_.load(); }
    std::string Status() const {
        std::lock_guard<std::mutex> lock(statusMutex_);
        return status_;
    }

private:
    void SetStatus(std::string value) {
        std::lock_guard<std::mutex> lock(statusMutex_);
        status_ = std::move(value);
        std::cout << "[network] " << status_ << std::endl;
    }

    void Run() {
        SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        listeningSocket_.store(server);
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
        address.sin_port = htons(port_);
        if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
            listen(server, 1) == SOCKET_ERROR) {
            SetStatus("Could not listen on TCP port " + std::to_string(port_));
            closesocket(server);
            listeningSocket_.store(INVALID_SOCKET);
            return;
        }
        SetStatus("Waiting for phone on TCP port " + std::to_string(port_));
        while (running_.load()) {
            SOCKET client = accept(server, nullptr, nullptr);
            if (client == INVALID_SOCKET) break;
            SetStatus("TCP client connected; waiting for pairing handshake");
            clientSocket_.store(client);
            BOOL noDelay = TRUE;
            setsockopt(client, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
            HandleClient(client);
            connected_.store(false);
            closesocket(client);
            clientSocket_.store(INVALID_SOCKET);
            if (running_.load()) SetStatus("Phone disconnected; waiting for reconnection");
        }
    }

    void HandleClient(SOCKET client) {
        bool authenticated = false;
        std::array<std::uint8_t, phonecast::core::protocol::kHeaderSize> header{};
        while (running_.load() && ReceiveExact(client, header.data(), header.size())) {
            Message message;
            std::uint32_t payloadSize = 0;
            std::string error;
            if (!phonecast::core::protocol::ParseHeader(header.data(), header.size(), message,
                                                         payloadSize, error)) {
                SetStatus(error);
                return;
            }
            message.payload.resize(payloadSize);
            if (payloadSize > 0 && !ReceiveExact(client, message.payload.data(), payloadSize)) return;
            if (!authenticated) {
                const std::string supplied(message.payload.begin(), message.payload.end());
                if (message.type != MessageType::Hello || supplied != pairCode_) {
                    SetStatus("Rejected a client with an invalid pairing code");
                    return;
                }
                authenticated = true;
                connected_.store(true);
                SetStatus("Phone connected and authenticated");
                continue;
            }
            if (message.type == MessageType::EndStream) return;
            if (message.type == MessageType::Ping) {
                Message pong;
                pong.type = MessageType::Pong;
                pong.timestampMicros = message.timestampMicros;
                if (!SendExact(client, phonecast::core::protocol::Serialize(pong))) return;
                continue;
            }
            if (message.type == MessageType::VideoConfig || message.type == MessageType::VideoFrame) {
                queue_.Push(std::move(message));
            }
        }
    }

    MessageQueue& queue_;
    std::string pairCode_;
    std::uint16_t port_;
    std::atomic_bool running_{};
    std::atomic_bool connected_{};
    std::atomic<SOCKET> listeningSocket_{INVALID_SOCKET};
    std::atomic<SOCKET> clientSocket_{INVALID_SOCKET};
    std::thread thread_;
    mutable std::mutex statusMutex_;
    std::string status_;
};

void PrintUsage() {
    std::cout << "Usage: phonecast-stream-receiver --pair-code NNNNNN [--port N]\n";
}
}  // namespace

int main(int argc, char** argv) {
    std::string pairCode;
    std::uint16_t port = 49321;
    for (int index = 1; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--help" || option == "-h") {
            PrintUsage();
            return EXIT_SUCCESS;
        }
        if (index + 1 >= argc) {
            PrintUsage();
            return EXIT_FAILURE;
        }
        const std::string value = argv[++index];
        if (option == "--pair-code") pairCode = value;
        else if (option == "--port") {
            try {
                const unsigned long parsed = std::stoul(value);
                if (parsed == 0 || parsed > 65535) throw std::out_of_range("port");
                port = static_cast<std::uint16_t>(parsed);
            } catch (...) {
                std::cerr << "Invalid TCP port.\n";
                return EXIT_FAILURE;
            }
        } else {
            std::cerr << "Unknown option: " << option << '\n';
            return EXIT_FAILURE;
        }
    }
    if (!phonecast::core::protocol::IsValidPairCode(pairCode)) {
        std::cerr << "A six-digit --pair-code is required.\n";
        PrintUsage();
        return EXIT_FAILURE;
    }

    phonecast::platform::windows::DesktopPreview preview;
    std::string error;
    if (!preview.Start(error)) {
        std::cerr << error << '\n';
        return EXIT_FAILURE;
    }
    MessageQueue queue;
    TcpServer server(queue, pairCode, port);
    if (!server.Start(error)) {
        std::cerr << error << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "PhoneCast receiver listening on port " << port
              << ". Pairing code: " << pairCode << '\n';

    phonecast::platform::windows::MfH264Decoder decoder;
    bool decoderStarted = false;
    std::vector<std::uint8_t> codecConfig;
    std::uint32_t streamWidth = 0;
    std::uint32_t streamHeight = 0;
    std::uint64_t decodedFrames = 0;
    std::uint64_t receivedBytes = 0;
    std::uint64_t lastSequence = 0;
    std::uint64_t missingFrames = 0;
    double totalDecodeMillis = 0.0;
    auto statsStarted = std::chrono::steady_clock::now();
    auto lastTitle = statsStarted;

    while (preview.PumpEvents()) {
        Message message;
        while (queue.Pop(message)) {
            receivedBytes += message.payload.size();
            if (message.type == MessageType::VideoConfig) {
                codecConfig = std::move(message.payload);
                streamWidth = message.width;
                streamHeight = message.height;
                decoderStarted = decoder.Start(streamWidth, streamHeight, error);
                if (!decoderStarted) std::cerr << error << '\n';
                continue;
            }
            if (!decoderStarted) continue;
            if (decodedFrames > 0 && message.sequence > lastSequence + 1) {
                missingFrames += message.sequence - lastSequence - 1;
            }
            lastSequence = message.sequence;
            std::vector<std::uint8_t> accessUnit;
            if ((message.flags & phonecast::core::protocol::MessageFlags::KeyFrame) != 0) {
                accessUnit.reserve(codecConfig.size() + message.payload.size());
                accessUnit.insert(accessUnit.end(), codecConfig.begin(), codecConfig.end());
                accessUnit.insert(accessUnit.end(), message.payload.begin(), message.payload.end());
            } else {
                accessUnit = std::move(message.payload);
            }
            phonecast::core::VideoFrame frame;
            bool produced = false;
            const auto decodeStarted = std::chrono::steady_clock::now();
            if (!decoder.Submit(accessUnit, message.timestampMicros, frame, produced, error)) {
                std::cerr << error << '\n';
                decoderStarted = false;
            } else if (produced) {
                totalDecodeMillis += std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - decodeStarted).count();
                preview.Present(frame);
                ++decodedFrames;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - lastTitle >= std::chrono::seconds(1)) {
            const double seconds = std::chrono::duration<double>(now - statsStarted).count();
            const double fps = seconds > 0 ? decodedFrames / seconds : 0;
            const double megabits = seconds > 0 ? receivedBytes * 8.0 / seconds / 1'000'000.0 : 0;
            preview.SetTitle("PhoneCast | " + server.Status() + " | " +
                std::to_string(streamWidth) + "x" + std::to_string(streamHeight) +
                " | " + std::to_string(fps).substr(0, 4) + " FPS | " +
                std::to_string(megabits).substr(0, 4) + " Mbps | decode " +
                std::to_string(decodedFrames > 0 ? totalDecodeMillis / decodedFrames : 0.0).substr(0, 4) +
                " ms | dropped " + std::to_string(queue.Dropped() + missingFrames));
            lastTitle = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    server.Stop();
    decoder.Stop();
    preview.Stop();
    return EXIT_SUCCESS;
}
