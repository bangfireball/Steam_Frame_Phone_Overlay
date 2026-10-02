#include <openvr.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

constexpr char kApplicationKey[] = "com.phonecastvr.hello-frame";
constexpr char kOverlayKey[] = "com.phonecastvr.hello-frame.panel";
constexpr char kOverlayName[] = "PhoneCast VR - Hello Frame";
constexpr std::uint32_t kTextureWidth = 640;
constexpr std::uint32_t kTextureHeight = 240;
constexpr float kOverlayWidthMeters = 0.55F;

std::atomic_bool g_running{true};

void OnSignal(int) {
    g_running.store(false);
}

void Log(const char* level, const std::string& message) {
    std::cout << '[' << level << "] " << message << '\n';
}

const char* OverlayErrorName(vr::EVROverlayError error) {
    const char* name = vr::VROverlay()->GetOverlayErrorNameFromEnum(error);
    return name != nullptr ? name : "Unknown overlay error";
}

bool CheckOverlay(vr::EVROverlayError error, const char* operation) {
    if (error == vr::VROverlayError_None) {
        return true;
    }

    std::cerr << "[error] " << operation << " failed: "
              << OverlayErrorName(error) << " (" << static_cast<int>(error) << ")\n";
    return false;
}

struct Image {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> rgba;
};

using Glyph = std::array<const char*, 7>;

const std::unordered_map<char, Glyph> kGlyphs{
    {'A', {"01110", "10001", "10001", "11111", "10001", "10001", "10001"}},
    {'E', {"11111", "10000", "10000", "11110", "10000", "10000", "11111"}},
    {'F', {"11111", "10000", "10000", "11110", "10000", "10000", "10000"}},
    {'H', {"10001", "10001", "10001", "11111", "10001", "10001", "10001"}},
    {'L', {"10000", "10000", "10000", "10000", "10000", "10000", "11111"}},
    {'M', {"10001", "11011", "10101", "10101", "10001", "10001", "10001"}},
    {'O', {"01110", "10001", "10001", "10001", "10001", "10001", "01110"}},
    {'R', {"11110", "10001", "10001", "11110", "10100", "10010", "10001"}},
};

void SetPixel(Image& image, int x, int y, const std::array<std::uint8_t, 4>& color) {
    if (x < 0 || y < 0 || x >= static_cast<int>(image.width) ||
        y >= static_cast<int>(image.height)) {
        return;
    }

    const auto index = (static_cast<std::size_t>(y) * image.width +
                        static_cast<std::size_t>(x)) * 4U;
    std::copy(color.begin(), color.end(), image.rgba.begin() + static_cast<std::ptrdiff_t>(index));
}

void FillRect(Image& image, int x, int y, int width, int height,
              const std::array<std::uint8_t, 4>& color) {
    for (int row = y; row < y + height; ++row) {
        for (int column = x; column < x + width; ++column) {
            SetPixel(image, column, row, color);
        }
    }
}

Image MakeHelloFrameImage() {
    constexpr std::array<std::uint8_t, 4> background{10, 16, 28, 255};
    constexpr std::array<std::uint8_t, 4> border{42, 188, 255, 255};
    constexpr std::array<std::uint8_t, 4> text{245, 249, 255, 255};
    constexpr int scale = 8;
    constexpr int glyphWidth = 5 * scale;
    constexpr int spacing = scale;
    constexpr char label[] = "HELLO FRAME";

    Image image{kTextureWidth, kTextureHeight,
                std::vector<std::uint8_t>(kTextureWidth * kTextureHeight * 4U)};

    FillRect(image, 0, 0, static_cast<int>(image.width), static_cast<int>(image.height), background);
    FillRect(image, 0, 0, static_cast<int>(image.width), 6, border);
    FillRect(image, 0, static_cast<int>(image.height) - 6, static_cast<int>(image.width), 6, border);
    FillRect(image, 0, 0, 6, static_cast<int>(image.height), border);
    FillRect(image, static_cast<int>(image.width) - 6, 0, 6, static_cast<int>(image.height), border);

    const int characterCount = static_cast<int>(sizeof(label) - 1);
    const int textWidth = characterCount * glyphWidth + (characterCount - 1) * spacing;
    const int originX = (static_cast<int>(image.width) - textWidth) / 2;
    const int originY = (static_cast<int>(image.height) - 7 * scale) / 2;

    int cursorX = originX;
    for (const char character : std::string(label)) {
        if (character != ' ') {
            const auto glyph = kGlyphs.find(character);
            if (glyph != kGlyphs.end()) {
                for (int row = 0; row < 7; ++row) {
                    for (int column = 0; column < 5; ++column) {
                        if (glyph->second[static_cast<std::size_t>(row)][column] == '1') {
                            FillRect(image, cursorX + column * scale, originY + row * scale,
                                     scale, scale, text);
                        }
                    }
                }
            }
        }
        cursorX += glyphWidth + spacing;
    }

    return image;
}

int ParseDuration(int argc, char** argv) {
    int durationSeconds = 0;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help") {
            std::cout << "Usage: hello-frame [--duration-seconds N]\n"
                         "Without a duration, the overlay runs until Ctrl+C or SteamVR quits.\n";
            std::exit(EXIT_SUCCESS);
        }
        if (argument == "--duration-seconds" && index + 1 < argc) {
            try {
                durationSeconds = std::stoi(argv[++index]);
            } catch (const std::exception&) {
                std::cerr << "[error] Invalid duration.\n";
                std::exit(EXIT_FAILURE);
            }
            if (durationSeconds < 0) {
                std::cerr << "[error] Duration must be zero or greater.\n";
                std::exit(EXIT_FAILURE);
            }
        } else {
            std::cerr << "[error] Unknown or incomplete argument: " << argument << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
    return durationSeconds;
}

bool IsQuitEvent(std::uint32_t eventType) {
    // ProcessQuit may describe an unrelated scene process during an application transition.
    return eventType == vr::VREvent_Quit || eventType == vr::VREvent_DriverRequestedQuit;
}

void RegisterApplicationManifest(vr::IVRApplications* applications, const char* executablePath) {
    if (applications == nullptr) {
        Log("warning", "OpenVR did not provide IVRApplications; manifest was not registered.");
        return;
    }

    std::error_code pathError;
    const auto absoluteExecutable = std::filesystem::absolute(executablePath, pathError);
    if (pathError) {
        Log("warning", "Could not resolve the executable path; manifest was not registered.");
        return;
    }

    const auto manifest = absoluteExecutable.parent_path() / "hello-frame.vrmanifest";
    if (!std::filesystem::exists(manifest)) {
        Log("warning", "Application manifest not found next to the executable: " + manifest.string());
        return;
    }

    const vr::EVRApplicationError error =
        applications->AddApplicationManifest(manifest.string().c_str(), false);
    if (error != vr::VRApplicationError_None) {
        const char* errorName = applications->GetApplicationsErrorNameFromEnum(error);
        Log("warning", "AddApplicationManifest failed: " +
                           std::string(errorName != nullptr ? errorName : "unknown error") +
                           " (" + std::to_string(static_cast<int>(error)) + ")");
        return;
    }

    if (!applications->IsApplicationInstalled(kApplicationKey)) {
        Log("warning", "SteamVR accepted the manifest path but did not install " +
                           std::string(kApplicationKey) + ". Check the runtime log for schema errors.");
        return;
    }

    Log("info", "Registered application manifest for " + std::string(kApplicationKey) + '.');
}

}  // namespace

int main(int argc, char** argv) {
    const int durationSeconds = ParseDuration(argc, argv);
    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    Log("info", "Starting PhoneCast VR Sprint 0 Hello Frame experiment.");
    Log("info", "OpenVR SDK: 2.15.6; overlay key: " + std::string(kOverlayKey));

    vr::EVRInitError initError = vr::VRInitError_None;
    vr::IVRSystem* vrSystem = vr::VR_Init(&initError, vr::VRApplication_Overlay);
    if (initError != vr::VRInitError_None || vrSystem == nullptr) {
        std::cerr << "[error] VR_Init failed: "
                  << vr::VR_GetVRInitErrorAsEnglishDescription(initError)
                  << " (" << static_cast<int>(initError) << ")\n";
        return EXIT_FAILURE;
    }

    RegisterApplicationManifest(vr::VRApplications(), argv[0]);

    vr::IVROverlay* overlayApi = vr::VROverlay();
    if (overlayApi == nullptr) {
        std::cerr << "[error] OpenVR did not provide IVROverlay.\n";
        vr::VR_Shutdown();
        return EXIT_FAILURE;
    }

    vr::VROverlayHandle_t overlay = vr::k_ulOverlayHandleInvalid;
    if (!CheckOverlay(overlayApi->CreateOverlay(kOverlayKey, kOverlayName, &overlay),
                      "CreateOverlay")) {
        vr::VR_Shutdown();
        return EXIT_FAILURE;
    }

    bool configured = true;
    configured &= CheckOverlay(overlayApi->SetOverlayWidthInMeters(overlay, kOverlayWidthMeters),
                               "SetOverlayWidthInMeters");
    configured &= CheckOverlay(overlayApi->SetOverlayAlpha(overlay, 1.0F), "SetOverlayAlpha");

    vr::HmdMatrix34_t transform{{
        {1.0F, 0.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 1.0F, -1.0F},
    }};
    configured &= CheckOverlay(
        overlayApi->SetOverlayTransformTrackedDeviceRelative(
            overlay, vr::k_unTrackedDeviceIndex_Hmd, &transform),
        "SetOverlayTransformTrackedDeviceRelative");

    Image image = MakeHelloFrameImage();
    configured &= CheckOverlay(
        overlayApi->SetOverlayRaw(overlay, image.rgba.data(), image.width, image.height, 4),
        "SetOverlayRaw");
    configured &= CheckOverlay(overlayApi->ShowOverlay(overlay), "ShowOverlay");

    if (!configured) {
        overlayApi->DestroyOverlay(overlay);
        vr::VR_Shutdown();
        return EXIT_FAILURE;
    }

    Log("info", "Overlay is visible: 640x240 RGBA, 0.55 m wide, 1.0 m ahead of the HMD.");
    if (durationSeconds > 0) {
        Log("info", "Automatic shutdown after " + std::to_string(durationSeconds) + " seconds.");
    } else {
        Log("info", "Press Ctrl+C to stop.");
    }

    const auto started = std::chrono::steady_clock::now();
    bool lastVisibility = overlayApi->IsOverlayVisible(overlay);
    while (g_running.load()) {
        vr::VREvent_t event{};
        while (overlayApi->PollNextOverlayEvent(overlay, &event, sizeof(event))) {
            if (event.eventType == vr::VREvent_ImageLoaded) {
                Log("info", "SteamVR reported that the raw overlay image loaded.");
            } else if (event.eventType == vr::VREvent_ImageFailed) {
                Log("error", "SteamVR reported that the raw overlay image failed to load.");
            } else if (IsQuitEvent(event.eventType)) {
                Log("info", "SteamVR requested overlay shutdown.");
                g_running.store(false);
            }
        }

        while (vrSystem->PollNextEvent(&event, sizeof(event))) {
            if (IsQuitEvent(event.eventType)) {
                Log("info", "SteamVR requested process shutdown.");
                vrSystem->AcknowledgeQuit_Exiting();
                g_running.store(false);
            }
        }

        const bool visible = overlayApi->IsOverlayVisible(overlay);
        if (visible != lastVisibility) {
            Log("info", std::string("Overlay visibility changed: ") +
                            (visible ? "visible" : "hidden"));
            lastVisibility = visible;
        }

        if (durationSeconds > 0 &&
            std::chrono::steady_clock::now() - started >=
                std::chrono::seconds(durationSeconds)) {
            Log("info", "Configured duration elapsed.");
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    overlayApi->HideOverlay(overlay);
    overlayApi->DestroyOverlay(overlay);
    vr::VR_Shutdown();
    Log("info", "Hello Frame stopped cleanly.");
    return EXIT_SUCCESS;
}
