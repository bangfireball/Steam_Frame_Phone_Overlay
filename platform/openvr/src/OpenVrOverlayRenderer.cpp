#include "phonecast/platform/openvr/OpenVrOverlayRenderer.h"

#include <openvr.h>

#include <filesystem>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#endif

namespace phonecast::platform::openvr {
namespace vr = ::vr;
namespace {

constexpr char kApplicationKey[] = "com.phonecastvr.receiver";
constexpr char kOverlayKey[] = "com.phonecastvr.receiver.phone";
constexpr char kOverlayName[] = "PhoneCast VR Receiver";

bool IsQuitEvent(std::uint32_t type) {
    // VREvent_ProcessQuit reports that some VR process exited; it is not a request
    // for this overlay process to stop during scene-application transitions.
    return type == vr::VREvent_Quit || type == vr::VREvent_DriverRequestedQuit;
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

    bool ApplySettings(const phonecast::vr::OverlaySettings& settings, std::string& error) {
        vr::HmdMatrix34_t transform{{
            {1.0F, 0.0F, 0.0F, settings.offsetXMeters},
            {0.0F, 1.0F, 0.0F, settings.offsetYMeters},
            {0.0F, 0.0F, 1.0F, -settings.distanceMeters},
        }};
        return OverlayCall(overlayApi->SetOverlayWidthInMeters(overlay, settings.widthMeters),
                           "SetOverlayWidthInMeters", error) &&
               OverlayCall(overlayApi->SetOverlayAlpha(overlay, settings.alpha),
                           "SetOverlayAlpha", error) &&
               OverlayCall(overlayApi->SetOverlayTransformTrackedDeviceRelative(
                               overlay, vr::k_unTrackedDeviceIndex_Hmd, &transform),
                           "SetOverlayTransformTrackedDeviceRelative", error);
    }

#ifdef _WIN32
    bool EnsureTexture(std::uint32_t width, std::uint32_t height, std::string& error) {
        if (texture != nullptr && textureWidth == width && textureHeight == height) return true;
        if (texture != nullptr) {
            texture->Release();
            texture = nullptr;
        }
        textureWidth = 0;
        textureHeight = 0;
        if (device == nullptr) {
            const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
            D3D_FEATURE_LEVEL selected{};
            HRESULT result = E_FAIL;
            int32_t adapterIndex = -1;
            if (system != nullptr) system->GetDXGIOutputInfo(&adapterIndex);
            IDXGIFactory1* factory = nullptr;
            IDXGIAdapter1* adapter = nullptr;
            if (adapterIndex >= 0 && SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) &&
                SUCCEEDED(factory->EnumAdapters1(static_cast<UINT>(adapterIndex), &adapter))) {
                result = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
                                           nullptr, 0, D3D11_SDK_VERSION, &device,
                                           &selected, &context);
            }
            if (adapter != nullptr) adapter->Release();
            if (factory != nullptr) factory->Release();
            if (FAILED(result)) {
                logger.Log(core::LogLevel::Warning, "openvr",
                           "Could not create D3D11 on SteamVR's compositor adapter; trying the default adapter.");
                result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                           nullptr, 0, D3D11_SDK_VERSION, &device,
                                           &selected, &context);
            }
            if (FAILED(result)) {
                error = "D3D11CreateDevice failed (HRESULT " + std::to_string(result) + ").";
                return false;
            }
            logger.Log(core::LogLevel::Info, "openvr",
                       "Created the overlay texture device on DXGI adapter " +
                       std::to_string(adapterIndex) + '.');
        }
        D3D11_TEXTURE2D_DESC description{};
        description.Width = width;
        description.Height = height;
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        const HRESULT result = device->CreateTexture2D(&description, nullptr, &texture);
        if (FAILED(result)) {
            error = "Creating the OpenVR D3D11 texture failed (HRESULT " +
                    std::to_string(result) + ").";
            return false;
        }
        textureWidth = width;
        textureHeight = height;
        return true;
    }
#endif

    core::ILogger& logger;
    std::string executablePath;
    vr::IVRSystem* system{nullptr};
    vr::IVROverlay* overlayApi{nullptr};
    vr::VROverlayHandle_t overlay{vr::k_ulOverlayHandleInvalid};
    bool shown{false};
    bool desiredVisible{true};
    bool hasFrame{false};
#ifdef _WIN32
    ID3D11Device* device{nullptr};
    ID3D11DeviceContext* context{nullptr};
    ID3D11Texture2D* texture{nullptr};
    std::uint32_t textureWidth{};
    std::uint32_t textureHeight{};
#endif
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

    if (!impl_->ApplySettings(settings, error)) {
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
#ifdef _WIN32
    if (!impl_->EnsureTexture(frame.width, frame.height, error)) return false;
    impl_->context->UpdateSubresource(impl_->texture, 0, nullptr, frame.pixels.data(),
                                     frame.width * 4U, 0);
    impl_->context->Flush();
    vr::Texture_t texture{impl_->texture, vr::TextureType_DirectX, vr::ColorSpace_Auto};
    if (!impl_->OverlayCall(impl_->overlayApi->SetOverlayTexture(impl_->overlay, &texture),
                            "SetOverlayTexture", error)) return false;
#else
    if (!impl_->OverlayCall(impl_->overlayApi->SetOverlayRaw(
                                impl_->overlay, const_cast<std::uint8_t*>(frame.pixels.data()),
                                frame.width, frame.height, 4),
                            "SetOverlayRaw", error)) return false;
#endif
    impl_->hasFrame = true;
    if (impl_->desiredVisible && !impl_->shown) {
        if (!SetVisible(true, error)) return false;
        impl_->logger.Log(core::LogLevel::Info, "openvr", "Overlay is visible.");
    }
    error.clear();
    return true;
}

bool OpenVrOverlayRenderer::ApplySettings(const phonecast::vr::OverlaySettings& settings,
                                          std::string& error) {
    if (impl_->overlayApi == nullptr || impl_->overlay == vr::k_ulOverlayHandleInvalid) {
        error = "OpenVR overlay is not started.";
        return false;
    }
    if (!impl_->ApplySettings(settings, error)) return false;
    error.clear();
    return true;
}

bool OpenVrOverlayRenderer::SetVisible(bool visible, std::string& error) {
    impl_->desiredVisible = visible;
    if (impl_->overlayApi == nullptr || impl_->overlay == vr::k_ulOverlayHandleInvalid) {
        error = "OpenVR overlay is not started.";
        return false;
    }
    if (visible && impl_->hasFrame) {
        if (!impl_->OverlayCall(impl_->overlayApi->ShowOverlay(impl_->overlay),
                                "ShowOverlay", error)) return false;
        impl_->shown = true;
    } else if (!visible && impl_->shown) {
        if (!impl_->OverlayCall(impl_->overlayApi->HideOverlay(impl_->overlay),
                                "HideOverlay", error)) return false;
        impl_->shown = false;
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
    impl_->desiredVisible = true;
    impl_->hasFrame = false;
#ifdef _WIN32
    if (impl_->texture != nullptr) impl_->texture->Release();
    if (impl_->context != nullptr) impl_->context->Release();
    if (impl_->device != nullptr) impl_->device->Release();
    impl_->texture = nullptr;
    impl_->context = nullptr;
    impl_->device = nullptr;
    impl_->textureWidth = 0;
    impl_->textureHeight = 0;
#endif
    if (impl_->system != nullptr) vr::VR_Shutdown();
    impl_->system = nullptr;
}

}  // namespace phonecast::platform::openvr
