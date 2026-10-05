#pragma once

#include "phonecast/core/logging/ILogger.h"

#include <openvr.h>

#include <cstdint>
#include <memory>
#include <string>

namespace phonecast::platform::openvr {

// Linux/Steam Frame-specific reusable texture uploader. Vulkan handles stay
// entirely behind this platform boundary; Core continues to expose RGBA frames.
class SteamFrameVulkanTexture {
public:
    explicit SteamFrameVulkanTexture(core::ILogger& logger);
    ~SteamFrameVulkanTexture();

    SteamFrameVulkanTexture(const SteamFrameVulkanTexture&) = delete;
    SteamFrameVulkanTexture& operator=(const SteamFrameVulkanTexture&) = delete;

    bool Initialize(::vr::IVRSystem* system, ::vr::IVRCompositor* compositor,
                    std::string& error);
    bool Update(::vr::IVROverlay* overlayApi, ::vr::VROverlayHandle_t overlay,
                const std::uint8_t* rgba, std::uint32_t width,
                std::uint32_t height, std::string& error);

    // Call only after the OpenVR texture has been cleared, its overlay has been
    // destroyed, and VR_Shutdown has returned.
    void Shutdown() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace phonecast::platform::openvr
