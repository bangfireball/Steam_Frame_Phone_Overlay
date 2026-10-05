#include "phonecast/platform/steamframe/StandaloneRuntime.h"

#include "phonecast/core/protocol/StreamProtocol.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <random>
#include <string>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

namespace phonecast::platform::steamframe {
namespace {

std::filesystem::path RuntimeDirectory() {
    const char* runtime = std::getenv("XDG_RUNTIME_DIR");
    if (runtime != nullptr && *runtime != '\0')
        return std::filesystem::path(runtime) / "phonecast-vr";
    return std::filesystem::path("/tmp") /
           ("phonecast-vr-" + std::to_string(static_cast<unsigned long>(getuid())));
}

bool SetSocketAddress(const std::filesystem::path& path, sockaddr_un& address,
                      std::string& error) {
    const std::string value = path.string();
    if (value.size() >= sizeof(address.sun_path)) {
        error = "PhoneCast runtime socket path is too long: " + value;
        return false;
    }
    address = {};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, value.c_str(), value.size() + 1U);
    return true;
}

bool SignalExisting(const std::filesystem::path& socketPath, std::string& error) {
    sockaddr_un address{};
    if (!SetSocketAddress(socketPath, address, error)) return false;
    for (int attempt = 0; attempt < 20; ++attempt) {
        const int socketFd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (socketFd < 0) {
            error = "Could not create the PhoneCast instance socket: " +
                    std::string(std::strerror(errno));
            return false;
        }
        if (connect(socketFd, reinterpret_cast<const sockaddr*>(&address),
                    sizeof(address)) == 0) {
            constexpr char command[] = "SHOW_DASHBOARD\n";
            const ssize_t sent = send(socketFd, command, sizeof(command) - 1U, MSG_NOSIGNAL);
            close(socketFd);
            if (sent == static_cast<ssize_t>(sizeof(command) - 1U)) return true;
            error = "The resident PhoneCast process did not accept the focus request.";
            return false;
        }
        close(socketFd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    error = "PhoneCast is already running, but its dashboard command socket is unavailable.";
    return false;
}

}  // namespace

class StandaloneRuntime::Impl {
public:
    std::filesystem::path directory;
    std::filesystem::path lockPath;
    std::filesystem::path socketPath;
    int lockFd{-1};
    int listenFd{-1};
};

StandaloneRuntime::StandaloneRuntime() : impl_(std::make_unique<Impl>()) {}
StandaloneRuntime::~StandaloneRuntime() { Stop(); }

InstanceStartResult StandaloneRuntime::Start(std::string& error) {
    if (impl_->lockFd >= 0) {
        error = "PhoneCast standalone runtime is already initialized.";
        return InstanceStartResult::Error;
    }
    impl_->directory = RuntimeDirectory();
    impl_->lockPath = impl_->directory / "receiver.lock";
    impl_->socketPath = impl_->directory / "receiver.sock";
    std::error_code filesystemError;
    std::filesystem::create_directories(impl_->directory, filesystemError);
    if (filesystemError) {
        error = "Could not create the PhoneCast runtime directory: " +
                filesystemError.message();
        return InstanceStartResult::Error;
    }
    struct stat directoryStatus{};
    if (lstat(impl_->directory.c_str(), &directoryStatus) != 0 ||
        !S_ISDIR(directoryStatus.st_mode) || directoryStatus.st_uid != getuid()) {
        error = "The PhoneCast runtime path is not a private directory owned by this user.";
        return InstanceStartResult::Error;
    }
    chmod(impl_->directory.c_str(), S_IRWXU);

    impl_->lockFd = open(impl_->lockPath.c_str(), O_CREAT | O_RDWR | O_CLOEXEC,
                         S_IRUSR | S_IWUSR);
    if (impl_->lockFd < 0) {
        error = "Could not open the PhoneCast instance lock: " +
                std::string(std::strerror(errno));
        return InstanceStartResult::Error;
    }
    if (flock(impl_->lockFd, LOCK_EX | LOCK_NB) != 0) {
        const bool signaled = SignalExisting(impl_->socketPath, error);
        close(impl_->lockFd);
        impl_->lockFd = -1;
        return signaled ? InstanceStartResult::ExistingSignaled
                        : InstanceStartResult::Error;
    }

    unlink(impl_->socketPath.c_str());
    impl_->listenFd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (impl_->listenFd < 0) {
        error = "Could not create the PhoneCast command socket: " +
                std::string(std::strerror(errno));
        Stop();
        return InstanceStartResult::Error;
    }
    sockaddr_un address{};
    if (!SetSocketAddress(impl_->socketPath, address, error) ||
        bind(impl_->listenFd, reinterpret_cast<const sockaddr*>(&address),
             sizeof(address)) != 0 ||
        listen(impl_->listenFd, 4) != 0) {
        if (error.empty())
            error = "Could not bind the PhoneCast command socket: " +
                    std::string(std::strerror(errno));
        Stop();
        return InstanceStartResult::Error;
    }
    chmod(impl_->socketPath.c_str(), S_IRUSR | S_IWUSR);
    error.clear();
    return InstanceStartResult::Primary;
}

bool StandaloneRuntime::TakeDashboardFocusRequest() {
    if (impl_->listenFd < 0) return false;
    bool requested = false;
    for (;;) {
        const int client = accept4(impl_->listenFd, nullptr, nullptr,
                                   SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client < 0) {
            if (errno == EINTR) continue;
            break;
        }
        std::array<char, 64> message{};
        const ssize_t received = recv(client, message.data(), message.size() - 1U, 0);
        close(client);
        if (received > 0 &&
            std::string(message.data(), static_cast<std::size_t>(received)) ==
                "SHOW_DASHBOARD\n")
            requested = true;
    }
    return requested;
}

void StandaloneRuntime::Stop() noexcept {
    if (impl_->listenFd >= 0) close(impl_->listenFd);
    impl_->listenFd = -1;
    if (!impl_->socketPath.empty()) unlink(impl_->socketPath.c_str());
    if (impl_->lockFd >= 0) {
        flock(impl_->lockFd, LOCK_UN);
        close(impl_->lockFd);
    }
    impl_->lockFd = -1;
}

bool LoadOrCreatePairCode(const std::filesystem::path& path,
                          std::string& pairCode,
                          std::string& error) {
    std::ifstream input(path);
    if (input) {
        std::getline(input, pairCode);
        chmod(path.c_str(), S_IRUSR | S_IWUSR);
        if (!phonecast::core::protocol::IsValidPairCode(pairCode)) {
            error = "The persisted PhoneCast pairing credential is invalid: " + path.string();
            return false;
        }
        error.clear();
        return true;
    }

    std::error_code filesystemError;
    const auto parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, filesystemError);
    if (filesystemError) {
        error = "Could not create the PhoneCast configuration directory: " +
                filesystemError.message();
        return false;
    }
    std::random_device random;
    std::uniform_int_distribution<int> distribution(0, 999999);
    const int value = distribution(random);
    std::array<char, 7> digits{};
    std::snprintf(digits.data(), digits.size(), "%06d", value);
    pairCode = digits.data();

    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
                        S_IRUSR | S_IWUSR);
    if (fd < 0) {
        // Another launch may have created it between the read and exclusive open.
        std::ifstream retry(path);
        std::getline(retry, pairCode);
        if (retry && phonecast::core::protocol::IsValidPairCode(pairCode)) return true;
        error = "Could not create the PhoneCast pairing credential: " +
                std::string(std::strerror(errno));
        return false;
    }
    const std::string contents = pairCode + "\n";
    const ssize_t written = write(fd, contents.data(), contents.size());
    fsync(fd);
    close(fd);
    if (written != static_cast<ssize_t>(contents.size())) {
        unlink(path.c_str());
        error = "Could not write the complete PhoneCast pairing credential.";
        return false;
    }
    error.clear();
    return true;
}

}  // namespace phonecast::platform::steamframe
