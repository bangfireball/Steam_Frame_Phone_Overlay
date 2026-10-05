#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace phonecast::platform::steamframe {

enum class InstanceStartResult {
    Primary,
    ExistingSignaled,
    Error
};

// Owns the per-user native receiver lock and a private Unix-domain command
// socket. A second launcher invocation asks the resident process to focus its
// dashboard instead of starting another decoder/server instance.
class StandaloneRuntime {
public:
    StandaloneRuntime();
    ~StandaloneRuntime();

    StandaloneRuntime(const StandaloneRuntime&) = delete;
    StandaloneRuntime& operator=(const StandaloneRuntime&) = delete;

    InstanceStartResult Start(std::string& error);
    bool TakeDashboardFocusRequest();
    void Stop() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// Loads the persistent six-digit receiver credential, or creates it with
// owner-only permissions on first launch. The current protocol remains a
// trusted-LAN pairing gate rather than encrypted device authentication.
bool LoadOrCreatePairCode(const std::filesystem::path& path,
                          std::string& pairCode,
                          std::string& error);

}  // namespace phonecast::platform::steamframe
