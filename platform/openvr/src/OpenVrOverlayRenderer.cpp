#include "phonecast/platform/openvr/OpenVrOverlayRenderer.h"

#include <openvr.h>

#include <filesystem>
#include <string>

namespace phonecast::platform::openvr {
namespace vr = ::vr;
namespace {

constexpr char kApplicationKey[] = "com.phonecastvr.receiver";
constexpr char kOverlayKey[] = "com.phonecastvr.receiver.phone";
constexpr char kOverlayName[] = "PhoneCast VR Receiver";

bool IsQuitEvent(std::uint32_t type) {
    return type == vr::VREvent_Quit || type == vr::VREvent_ProcessQuit ||
           type == vr::VREvent_DriverRequestedQuit;
}

}  // namespace

class OpenVrOverlayRenderer::Impl {
public:
    Impl(core::ILogger& logger, std::string executablePath)
        : logger(logger), executablePath(std::move(executablePath)) {}

    bool OverlayCall(vr::EVROverlayError result, const char* operation, std::string& error) {
        if (result == vr::VROverlayError_None) return true;
        const char* name = overlayApi != nullptr ? overlayApi->GetOverlayErrorNameFromEnum(result) : nullptr;
        error = std::string(operation) + " failed: " + (name != nullptr ? name : "unknown error") +
                " (" + std::to_string(static_cast<int>(result)) + ")";
        return false;
    }

    void RegisterManifest() {
        vr::IVRApplications* applications = vr::VRApplications();
        if (applications == nullptr) {
            logger.Log(core::LogLevel::Warning, "openvr", "IVRApplications is unavailable; manifest not registered.");
            return;
        }
        std::error_code pathError;
        const auto executable = std::filesystem::absolute(executablePath, pathError);
        const auto manifest = executable.parent_path() / "phonecast-receiver.vrmanifest";
        if (pathError || !std::filesystem::exists(manifest)) {
            logger.Log(core::LogLevel::Warning, "openvr", "Receiver manifest was not found beside the executable.");
            return;
        }
        const auto result = applications->AddApplicationManifest(manifest.string().c_str(), false);
        if (result != vr::VRApplicationError_None) {
            const char* name = applications->GetApplicationsErrorNameFromEnum(result);
            logger.Log(core::LogLevel::Warning, "openvr",
                       std::string("Manifest registration failed: ") + (name != nullptr ? name : "unknown error"));
            return;
        }
        logger.Log(core::LogLevel::Info, "openvr", std::string("Registered ") + kApplicationKey + '.');
    }

    core::ILogger& logger;
    std::string executablePath;
    vr::IVRSystem* system{nullptr};
    vr::IVROverlay* overlayApi{nullptr};
    vr::VROverlayHandle_t overlay{vr::k_ulOverlayHandleInvalid};
    bool shown{false};
};

OpenVrOverlayRenderer::OpenVrOverlayRenderer(core::ILogger& logger, std::string executablePath)
    : impl_(std::make_unique<Impl>(logger, std::move(executablePath))) {}

OpenVrOverlayRenderer::~OpenVrOverlayRenderer() { Stop(); }

bool OpenVrOverlayRenderer::Start(const phonecast::vr::OverlaySettings& settings, std::string& error) {
    if (impl_->system != nullptr) {
        error = "OpenVR renderer is already started.";
        return false;
    }
    vr::EVRInitError initError = vr::VRInitError_None;
    impl_->system = vr::VR_Init(&initError, vr::VRApplication_Overlay);
    if (initError != vr::VRInitError_None || impl_->system == nullptr) {
        error = std::string("VR_Init failed: ") + vr::VR_GetVRInitErrorAsEnglishDescription(initError) +
                " (" + std::to_string(static_cast<int>(initError)) + ")";
        if (impl_->system != nullptr) vr::VR_Shutdown();
        impl_->system = nullptr;
        return false;
    }

    impl_->RegisterManifest();
    impl_->overlayApi = vr::VROverlay();
    if (impl_->overlayApi == nullptr) {
        error = "OpenVR did not provide IVROverlay.";
        Stop();
        return false;
    }
    if (!impl_->OverlayCall(impl_->overlayApi->CreateOverlay(kOverlayKey, kOverlayName, &impl_->overlay),
                            "CreateOverlay", error)) {
        Stop();
        return false;
    }

    vr::HmdMatrix34_t transform{{
        {1.0F, 0.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 1.0F, -settings.distanceMeters},
    }};
    if (!impl_->OverlayCall(impl_->overlayApi->SetOverlayWidthInMeters(impl_->overlay, settings.widthMeters),
                            "SetOverlayWidthInMeters", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayAlpha(impl_->overlay, settings.alpha),
                            "SetOverlayAlpha", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayTransformTrackedDeviceRelative(
                                impl_->overlay, vr::k_unTrackedDeviceIndex_Hmd, &transform),
                            "SetOverlayTransformTrackedDeviceRelative", error)) {
        Stop();
        return false;
    }
    impl_->logger.Log(core::LogLevel::Info, "openvr", "Overlay created; waiting for the first frame.");
    error.clear();
    return true;
}

bool OpenVrOverlayRenderer::SubmitFrame(const core::VideoFrame& frame, std::string& error) {
    if (impl_->overlayApi == nullptr || impl_->overlay == vr::k_ulOverlayHandleInvalid) {
        error = "OpenVR overlay is not started.";
        return false;
    }
    if (!frame.IsValid()) {
        error = "Cannot submit an invalid RGBA frame.";
        return false;
    }
    if (!impl_->OverlayCall(impl_->overlayApi->SetOverlayRaw(
                                impl_->overlay, const_cast<std::uint8_t*>(frame.pixels.data()),
                                frame.width, frame.height, 4),
                            "SetOverlayRaw", error)) return false;
    if (!impl_->shown) {
        if (!impl_->OverlayCall(impl_->overlayApi->ShowOverlay(impl_->overlay), "ShowOverlay", error)) {
            return false;
        }
        impl_->shown = true;
        impl_->logger.Log(core::LogLevel::Info, "openvr",
                          "Overlay visible with continuously generated RGBA frames.");
    }
    error.clear();
    return true;
}

bool OpenVrOverlayRenderer::PumpEvents() {
    if (impl_->overlayApi == nullptr || impl_->system == nullptr) return false;
    vr::VREvent_t event{};
    while (impl_->overlayApi->PollNextOverlayEvent(impl_->overlay, &event, sizeof(event))) {
        if (IsQuitEvent(event.eventType)) {
            impl_->logger.Log(core::LogLevel::Info, "openvr", "Runtime requested overlay shutdown.");
            return false;
        }
    }
    while (impl_->system->PollNextEvent(&event, sizeof(event))) {
        if (IsQuitEvent(event.eventType)) {
            impl_->system->AcknowledgeQuit_Exiting();
            impl_->logger.Log(core::LogLevel::Info, "openvr", "Runtime requested process shutdown.");
            return false;
        }
    }
    return true;
}

void OpenVrOverlayRenderer::Stop() noexcept {
    if (impl_->overlayApi != nullptr && impl_->overlay != vr::k_ulOverlayHandleInvalid) {
        impl_->overlayApi->HideOverlay(impl_->overlay);
        impl_->overlayApi->DestroyOverlay(impl_->overlay);
    }
    impl_->overlay = vr::k_ulOverlayHandleInvalid;
    impl_->overlayApi = nullptr;
    impl_->shown = false;
    if (impl_->system != nullptr) vr::VR_Shutdown();
    impl_->system = nullptr;
}

}  // namespace phonecast::platform::openvr
