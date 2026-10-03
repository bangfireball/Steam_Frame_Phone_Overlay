#include "phonecast/platform/openvr/OpenVrOverlayRenderer.h"

#include <openvr.h>

#include <algorithm>
#include <chrono>
#include <cmath>
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

constexpr float kPi = 3.14159265358979323846F;

float Clamp(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(maximum, value));
}

bool IsControllerMode(phonecast::vr::PlacementMode mode) {
    return mode == phonecast::vr::PlacementMode::LeftControllerLocked ||
           mode == phonecast::vr::PlacementMode::RightControllerLocked;
}

const phonecast::vr::ControllerCalibration& ControllerFor(
        const phonecast::vr::OverlaySettings& settings) {
    return settings.placementMode == phonecast::vr::PlacementMode::LeftControllerLocked
        ? settings.leftController : settings.rightController;
}

phonecast::vr::ControllerCalibration& ControllerFor(phonecast::vr::OverlaySettings& settings) {
    return settings.placementMode == phonecast::vr::PlacementMode::LeftControllerLocked
        ? settings.leftController : settings.rightController;
}

vr::HmdMatrix34_t LocalTransform(const phonecast::vr::OverlaySettings& settings) {
    return {{{1.0F, 0.0F, 0.0F, settings.offsetXMeters},
             {0.0F, 1.0F, 0.0F, settings.offsetYMeters},
             {0.0F, 0.0F, 1.0F, -settings.distanceMeters}}};
}

vr::HmdMatrix34_t Rotation(float tiltDegrees, float yawDegrees) {
    const float x = tiltDegrees * kPi / 180.0F;
    const float y = yawDegrees * kPi / 180.0F;
    const float cx = std::cos(x), sx = std::sin(x);
    const float cy = std::cos(y), sy = std::sin(y);
    return {{{cy, sy * sx, sy * cx, 0.0F},
             {0.0F, cx, -sx, 0.0F},
             {-sy, cy * sx, cy * cx, 0.0F}}};
}

vr::HmdMatrix34_t ControllerLocalTransform(
        const phonecast::vr::ControllerCalibration& calibration,
        float extraTilt = 0.0F, float extraYaw = 0.0F) {
    auto result = Rotation(calibration.tiltDegrees + extraTilt,
                           calibration.yawDegrees + extraYaw);
    result.m[0][3] = calibration.lateralMeters;
    result.m[1][3] = calibration.heightMeters;
    result.m[2][3] = -calibration.distanceMeters;
    return result;
}

vr::HmdMatrix34_t FromArray(const std::array<float, 12>& values) {
    vr::HmdMatrix34_t result{};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 4; ++column)
            result.m[row][column] = values[row * 4 + column];
    return result;
}

std::array<float, 12> ToArray(const vr::HmdMatrix34_t& matrix) {
    std::array<float, 12> result{};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 4; ++column)
            result[row * 4 + column] = matrix.m[row][column];
    return result;
}

vr::HmdMatrix34_t Multiply(const vr::HmdMatrix34_t& left, const vr::HmdMatrix34_t& right) {
    vr::HmdMatrix34_t result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            for (std::size_t inner = 0; inner < 3; ++inner)
                result.m[row][column] += left.m[row][inner] * right.m[inner][column];
        }
        result.m[row][3] = left.m[row][3];
        for (std::size_t inner = 0; inner < 3; ++inner)
            result.m[row][3] += left.m[row][inner] * right.m[inner][3];
    }
    return result;
}

vr::HmdMatrix34_t InverseRigid(const vr::HmdMatrix34_t& matrix) {
    vr::HmdMatrix34_t result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column)
            result.m[row][column] = matrix.m[column][row];
        for (std::size_t inner = 0; inner < 3; ++inner)
            result.m[row][3] -= result.m[row][inner] * matrix.m[inner][3];
    }
    return result;
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

    vr::TrackedDeviceIndex_t DeviceForMode(phonecast::vr::PlacementMode mode) const {
        if (mode == phonecast::vr::PlacementMode::HeadLocked)
            return vr::k_unTrackedDeviceIndex_Hmd;
        const auto role = mode == phonecast::vr::PlacementMode::LeftControllerLocked
            ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand;
        return system->GetTrackedDeviceIndexForControllerRole(role);
    }

    bool DevicePose(vr::TrackedDeviceIndex_t device, vr::HmdMatrix34_t& pose) const {
        if (device == vr::k_unTrackedDeviceIndexInvalid || device >= vr::k_unMaxTrackedDeviceCount)
            return false;
        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount]{};
        system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0F,
                                                poses, vr::k_unMaxTrackedDeviceCount);
        if (!poses[device].bPoseIsValid) return false;
        pose = poses[device].mDeviceToAbsoluteTracking;
        return true;
    }

    bool ControllerTransform(const phonecast::vr::OverlaySettings& settings,
                             vr::HmdMatrix34_t& absolute) const {
        const auto controller = DeviceForMode(settings.placementMode);
        const auto& calibration = ControllerFor(settings);
        vr::HmdMatrix34_t controllerPose{};
        if (!DevicePose(controller, controllerPose)) return false;

        if (calibration.orientation == phonecast::vr::ControllerOrientation::ControllerRelative) {
            absolute = Multiply(controllerPose, ControllerLocalTransform(calibration));
            return true;
        }
        if (calibration.orientation == phonecast::vr::ControllerOrientation::Wrist) {
            const float handYaw = settings.placementMode == phonecast::vr::PlacementMode::LeftControllerLocked
                ? 20.0F : -20.0F;
            absolute = Multiply(controllerPose, ControllerLocalTransform(calibration, -55.0F, handYaw));
            return true;
        }

        vr::HmdMatrix34_t hmdPose{};
        if (!DevicePose(vr::k_unTrackedDeviceIndex_Hmd, hmdPose)) return false;
        absolute = Multiply(controllerPose, ControllerLocalTransform(calibration));
        float normal[3]{hmdPose.m[0][3] - absolute.m[0][3],
                        hmdPose.m[1][3] - absolute.m[1][3],
                        hmdPose.m[2][3] - absolute.m[2][3]};
        if (calibration.orientation == phonecast::vr::ControllerOrientation::WorldUpright)
            normal[1] = 0.0F;
        const float normalLength = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] +
                                             normal[2] * normal[2]);
        if (normalLength < 0.001F) return false;
        for (float& component : normal) component /= normalLength;

        float right[3]{normal[2], 0.0F, -normal[0]};
        const float rightLength = std::sqrt(right[0] * right[0] + right[2] * right[2]);
        if (rightLength < 0.001F) {
            right[0] = 1.0F;
            right[1] = 0.0F;
            right[2] = 0.0F;
        } else {
            right[0] /= rightLength;
            right[2] /= rightLength;
        }
        const float up[3]{normal[1] * right[2] - normal[2] * right[1],
                          normal[2] * right[0] - normal[0] * right[2],
                          normal[0] * right[1] - normal[1] * right[0]};
        for (std::size_t row = 0; row < 3; ++row) {
            absolute.m[row][0] = right[row];
            absolute.m[row][1] = up[row];
            absolute.m[row][2] = normal[row];
        }
        const float px = absolute.m[0][3], py = absolute.m[1][3], pz = absolute.m[2][3];
        absolute = Multiply(absolute, Rotation(calibration.tiltDegrees, calibration.yawDegrees));
        absolute.m[0][3] = px;
        absolute.m[1][3] = py;
        absolute.m[2][3] = pz;
        return true;
    }

    bool AbsoluteForSettings(const phonecast::vr::OverlaySettings& settings,
                             vr::HmdMatrix34_t& absolute) const {
        if (settings.placementMode == phonecast::vr::PlacementMode::WorldLocked &&
            settings.worldTransformValid) {
            absolute = FromArray(settings.worldTransform);
            return true;
        }
        if (settings.placementMode == phonecast::vr::PlacementMode::LeftControllerLocked ||
            settings.placementMode == phonecast::vr::PlacementMode::RightControllerLocked)
            return ControllerTransform(settings, absolute);
        vr::HmdMatrix34_t devicePose{};
        if (!DevicePose(vr::k_unTrackedDeviceIndex_Hmd, devicePose)) return false;
        absolute = Multiply(devicePose, LocalTransform(settings));
        return true;
    }

    bool ApplySettings(phonecast::vr::OverlaySettings settings, std::string& error) {
        const float placementScale = IsControllerMode(settings.placementMode)
            ? ControllerFor(settings).scale : 1.0F;
        if (!OverlayCall(overlayApi->SetOverlayWidthInMeters(overlay, settings.widthMeters * placementScale),
                         "SetOverlayWidthInMeters", error) ||
            !OverlayCall(overlayApi->SetOverlayAlpha(overlay, settings.alpha),
                         "SetOverlayAlpha", error)) return false;

        if (settings.placementMode == phonecast::vr::PlacementMode::WorldLocked) {
            if (!settings.worldTransformValid) {
                vr::HmdMatrix34_t hmdPose{};
                if (!DevicePose(vr::k_unTrackedDeviceIndex_Hmd, hmdPose)) {
                    error = "Cannot create a world anchor because the HMD pose is unavailable.";
                    return false;
                }
                settings.worldTransform = ToArray(Multiply(hmdPose, LocalTransform(settings)));
                settings.worldTransformValid = true;
                pendingSettings = settings;
                hasPendingSettings = true;
            }
            auto transform = FromArray(settings.worldTransform);
            if (!OverlayCall(overlayApi->SetOverlayTransformAbsolute(
                                 overlay, vr::TrackingUniverseStanding, &transform),
                             "SetOverlayTransformAbsolute", error)) return false;
        } else if (settings.placementMode == phonecast::vr::PlacementMode::HeadLocked) {
            auto transform = LocalTransform(settings);
            if (!OverlayCall(overlayApi->SetOverlayTransformTrackedDeviceRelative(
                                 overlay, vr::k_unTrackedDeviceIndex_Hmd, &transform),
                             "SetOverlayTransformTrackedDeviceRelative", error)) return false;
        } else {
            vr::HmdMatrix34_t transform{};
            if (!ControllerTransform(settings, transform)) {
                error = "The selected VR controller or HMD pose is not available.";
                return false;
            }
            if (!OverlayCall(overlayApi->SetOverlayTransformAbsolute(
                                 overlay, vr::TrackingUniverseStanding, &transform),
                             "SetOverlayTransformAbsolute", error)) return false;
        }
        currentSettings = settings;
        return true;
    }

    void UpdateControllerPlacement() {
        if (grabbedDevice != vr::k_unTrackedDeviceIndexInvalid ||
            !IsControllerMode(currentSettings.placementMode)) return;
        vr::HmdMatrix34_t transform{};
        if (ControllerTransform(currentSettings, transform))
            overlayApi->SetOverlayTransformAbsolute(overlay, vr::TrackingUniverseStanding, &transform);
    }

    void PublishCalibration() {
        pendingSettings = currentSettings;
        hasPendingSettings = true;
        calibrationDirty = false;
    }

    void PollControllerCalibration() {
        const auto leftDevice = system->GetTrackedDeviceIndexForControllerRole(
            vr::TrackedControllerRole_LeftHand);
        const auto rightDevice = system->GetTrackedDeviceIndexForControllerRole(
            vr::TrackedControllerRole_RightHand);
        vr::VRControllerState_t leftState{};
        vr::VRControllerState_t rightState{};
        const bool haveLeft = leftDevice != vr::k_unTrackedDeviceIndexInvalid &&
            system->GetControllerState(leftDevice, &leftState, sizeof(leftState));
        const bool haveRight = rightDevice != vr::k_unTrackedDeviceIndexInvalid &&
            system->GetControllerState(rightDevice, &rightState, sizeof(rightState));
        const auto leftRising = haveLeft
            ? leftState.ulButtonPressed & ~previousLeftControllerButtons : 0;
        const auto rightRising = haveRight
            ? rightState.ulButtonPressed & ~previousRightControllerButtons : 0;
        previousLeftControllerButtons = haveLeft ? leftState.ulButtonPressed : 0;
        previousRightControllerButtons = haveRight ? rightState.ulButtonPressed : 0;
        const auto menuMask = vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu);
        const auto padMask = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad);
        if (!calibrationActive && (leftRising & padMask) != 0) {
            pendingGlanceInput = phonecast::vr::GlanceInput::LeftController;
            hasPendingGlanceInput = true;
        } else if (!calibrationActive && (rightRising & padMask) != 0) {
            pendingGlanceInput = phonecast::vr::GlanceInput::RightController;
            hasPendingGlanceInput = true;
        }
        const bool selectLeft = (leftRising & menuMask) != 0 &&
            currentSettings.placementMode != phonecast::vr::PlacementMode::LeftControllerLocked;
        const bool selectRight = (rightRising & menuMask) != 0 &&
            currentSettings.placementMode != phonecast::vr::PlacementMode::RightControllerLocked;
        if (selectLeft || selectRight) {
            if (calibrationDirty) PublishCalibration();
            currentSettings.placementMode = selectLeft
                ? phonecast::vr::PlacementMode::LeftControllerLocked
                : phonecast::vr::PlacementMode::RightControllerLocked;
            calibrationActive = true;
            std::string ignored;
            ApplySettings(currentSettings, ignored);
            PublishCalibration();
            logger.Log(core::LogLevel::Info, "openvr",
                       selectLeft ? "Left-controller calibration enabled."
                                  : "Right-controller calibration enabled.");
            return;
        }
        if (!IsControllerMode(currentSettings.placementMode)) {
            if (calibrationDirty) PublishCalibration();
            calibrationActive = false;
            return;
        }

        const bool leftSelected =
            currentSettings.placementMode == phonecast::vr::PlacementMode::LeftControllerLocked;
        if ((leftSelected && !haveLeft) || (!leftSelected && !haveRight)) return;
        const auto& state = leftSelected ? leftState : rightState;
        const auto pressed = state.ulButtonPressed;
        const auto rising = leftSelected ? leftRising : rightRising;
        const auto gripMask = vr::ButtonMaskFromId(vr::k_EButton_Grip);
        const auto triggerMask = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger);

        if ((rising & menuMask) != 0) {
            calibrationActive = !calibrationActive;
            if (!calibrationActive && calibrationDirty) PublishCalibration();
            logger.Log(core::LogLevel::Info, "openvr", calibrationActive
                ? "Controller calibration enabled: axis moves; grip+axis rotates; trigger+axis scales/distances; pad click changes orientation."
                : "Controller calibration disabled and saved.");
        }
        if (!calibrationActive) return;

        auto& calibration = ControllerFor(currentSettings);
        if ((rising & padMask) != 0) {
            if ((pressed & gripMask) != 0) {
                calibration = {};
                logger.Log(core::LogLevel::Info, "openvr", "Controller calibration reset for the active hand.");
            } else {
                calibration.orientation = static_cast<phonecast::vr::ControllerOrientation>(
                    (static_cast<int>(calibration.orientation) + 1) % 4);
                logger.Log(core::LogLevel::Info, "openvr", "Controller orientation mode changed.");
            }
            calibrationDirty = true;
            std::string ignored;
            ApplySettings(currentSettings, ignored);
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - lastCalibrationAdjustment < std::chrono::milliseconds(40)) return;
        lastCalibrationAdjustment = now;
        float x = state.rAxis[0].x;
        float y = state.rAxis[0].y;
        if (std::fabs(x) < 0.18F) x = 0.0F;
        if (std::fabs(y) < 0.18F) y = 0.0F;
        if (x == 0.0F && y == 0.0F) {
            if (calibrationDirty && (pressed & (gripMask | triggerMask | padMask)) == 0)
                PublishCalibration();
            return;
        }
        if ((pressed & gripMask) != 0) {
            calibration.yawDegrees = Clamp(calibration.yawDegrees + x * 2.0F, -180.0F, 180.0F);
            calibration.tiltDegrees = Clamp(calibration.tiltDegrees + y * 2.0F, -180.0F, 180.0F);
        } else if ((pressed & triggerMask) != 0) {
            calibration.scale = Clamp(calibration.scale + x * 0.02F, 0.25F, 3.0F);
            calibration.distanceMeters = Clamp(calibration.distanceMeters - y * 0.005F, 0.05F, 2.0F);
        } else {
            calibration.lateralMeters = Clamp(calibration.lateralMeters + x * 0.005F, -1.0F, 1.0F);
            calibration.heightMeters = Clamp(calibration.heightMeters + y * 0.005F, -1.0F, 1.0F);
        }
        calibrationDirty = true;
        std::string ignored;
        ApplySettings(currentSettings, ignored);
    }

    void BeginGrab(vr::TrackedDeviceIndex_t device) {
        vr::HmdMatrix34_t controllerPose{};
        vr::HmdMatrix34_t overlayPose{};
        if (!DevicePose(device, controllerPose) || !AbsoluteForSettings(currentSettings, overlayPose)) return;
        grabRelative = Multiply(InverseRigid(controllerPose), overlayPose);
        std::string ignored;
        if (OverlayCall(overlayApi->SetOverlayTransformTrackedDeviceRelative(
                            overlay, device, &grabRelative), "begin overlay grab", ignored)) {
            grabbedDevice = device;
            logger.Log(core::LogLevel::Info, "openvr", "Overlay grab started.");
        }
    }

    void EndGrab(vr::TrackedDeviceIndex_t device) {
        if (grabbedDevice == vr::k_unTrackedDeviceIndexInvalid || device != grabbedDevice) return;
        vr::HmdMatrix34_t controllerPose{};
        if (DevicePose(grabbedDevice, controllerPose)) {
            const auto absolute = Multiply(controllerPose, grabRelative);
            currentSettings.placementMode = phonecast::vr::PlacementMode::WorldLocked;
            currentSettings.worldTransform = ToArray(absolute);
            currentSettings.worldTransformValid = true;
            overlayApi->SetOverlayTransformAbsolute(overlay, vr::TrackingUniverseStanding, &absolute);
            pendingSettings = currentSettings;
            hasPendingSettings = true;
            logger.Log(core::LogLevel::Info, "openvr", "Overlay grab ended; placement is world-locked.");
        }
        grabbedDevice = vr::k_unTrackedDeviceIndexInvalid;
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
    phonecast::vr::OverlaySettings currentSettings{};
    phonecast::vr::OverlaySettings pendingSettings{};
    bool hasPendingSettings{false};
    phonecast::vr::GlanceInput pendingGlanceInput{phonecast::vr::GlanceInput::LeftController};
    bool hasPendingGlanceInput{false};
    bool calibrationActive{false};
    bool calibrationDirty{false};
    std::uint64_t previousLeftControllerButtons{0};
    std::uint64_t previousRightControllerButtons{0};
    std::chrono::steady_clock::time_point lastCalibrationAdjustment{};
    vr::TrackedDeviceIndex_t grabbedDevice{vr::k_unTrackedDeviceIndexInvalid};
    vr::HmdMatrix34_t grabRelative{};
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

    if (!impl_->OverlayCall(impl_->overlayApi->SetOverlayInputMethod(
                                impl_->overlay, vr::VROverlayInputMethod_Mouse),
                            "SetOverlayInputMethod", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayFlag(
                                impl_->overlay, vr::VROverlayFlags_MakeOverlaysInteractiveIfVisible, true),
                            "SetOverlayFlag", error) ||
        !impl_->ApplySettings(settings, error)) {
        Stop();
        return false;
    }
    impl_->logger.Log(core::LogLevel::Info, "openvr",
                      "Overlay created; point and hold trigger on it to grab and place it.");
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
    const vr::HmdVector2_t mouseScale{{static_cast<float>(frame.width),
                                      static_cast<float>(frame.height)}};
    if (!impl_->OverlayCall(impl_->overlayApi->SetOverlayMouseScale(impl_->overlay, &mouseScale),
                            "SetOverlayMouseScale", error)) return false;
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
    impl_->UpdateControllerPlacement();
    impl_->PollControllerCalibration();
    vr::VREvent_t event{};
    while (impl_->overlayApi->PollNextOverlayEvent(impl_->overlay, &event, sizeof(event))) {
        if (!impl_->calibrationActive && event.eventType == vr::VREvent_MouseButtonDown &&
            (event.data.mouse.button & vr::VRMouseButton_Left) != 0) {
            impl_->BeginGrab(event.trackedDeviceIndex);
        } else if (!impl_->calibrationActive && event.eventType == vr::VREvent_MouseButtonUp &&
                   (event.data.mouse.button & vr::VRMouseButton_Left) != 0) {
            impl_->EndGrab(event.trackedDeviceIndex);
        }
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

bool OpenVrOverlayRenderer::TakeSettingsUpdate(phonecast::vr::OverlaySettings& settings) {
    if (!impl_->hasPendingSettings) return false;
    settings = impl_->pendingSettings;
    impl_->hasPendingSettings = false;
    return true;
}

bool OpenVrOverlayRenderer::TakeGlanceInput(phonecast::vr::GlanceInput& input) {
    if (!impl_->hasPendingGlanceInput) return false;
    input = impl_->pendingGlanceInput;
    impl_->hasPendingGlanceInput = false;
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
    impl_->hasPendingSettings = false;
    impl_->hasPendingGlanceInput = false;
    impl_->calibrationActive = false;
    impl_->calibrationDirty = false;
    impl_->previousLeftControllerButtons = 0;
    impl_->previousRightControllerButtons = 0;
    impl_->grabbedDevice = vr::k_unTrackedDeviceIndexInvalid;
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
