#include "phonecast/platform/openvr/OpenVrOverlayRenderer.h"
#include "phonecast/vr/overlay/WristMenuGesture.h"

#include <openvr.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#else
#include <unistd.h>
#endif

namespace phonecast::platform::openvr {
namespace vr = ::vr;
namespace {

constexpr char kApplicationKey[] = "com.phonecastvr.receiver";
constexpr char kOverlayKey[] = "com.phonecastvr.receiver.phone";
constexpr char kOverlayName[] = "PhoneCast VR Receiver";
constexpr char kMenuOverlayKey[] = "com.phonecastvr.receiver.radial-menu";
constexpr char kMenuOverlayName[] = "PhoneCast Controls";
constexpr char kGestureOverlayKey[] = "com.phonecastvr.receiver.gesture-progress";
constexpr char kGestureOverlayName[] = "PhoneCast Gesture Progress";
constexpr char kSettingsOverlayKey[] = "com.phonecastvr.receiver.settings";
constexpr char kSettingsOverlayName[] = "PhoneCast Settings";
constexpr char kDashboardOverlayKey[] = "com.phonecastvr.receiver.dashboard";
constexpr char kDashboardOverlayName[] = "PhoneCast";
constexpr std::uint32_t kMenuTextureWidth = 512;
constexpr std::uint32_t kMenuTextureHeight = 896;
constexpr int kMenuColumns = 2;
constexpr int kMenuRows = 5;
constexpr int kCloseMenuCell = 8;
constexpr std::uint32_t kGestureTextureSize = 128;
constexpr int kGestureProgressSteps = 24;
constexpr std::uint32_t kSettingsTextureWidth = 512;
constexpr std::uint32_t kSettingsTextureHeight = 896;
constexpr std::uint32_t kDashboardTextureWidth = 1024;
constexpr std::uint32_t kDashboardTextureHeight = 512;
constexpr std::uint32_t kDashboardThumbnailSize = 256;
constexpr int kDashboardColumns = 4;
constexpr int kDashboardRows = 2;
// The dashboard is the approved in-headset entry point. Keep the earlier pose
// and thumbstick menu experiments dormant until product direction changes.
constexpr bool kEnableExperimentalGestureControls = false;
constexpr std::array<const char*, 9> kMenuLabels{
    "SHOW", "GLANCE", "PIN", "SETTINGS", "HEAD", "WORLD", "LEFT", "RIGHT", "CLOSE"};
constexpr std::array<phonecast::vr::RadialMenuAction, 8> kMenuActions{
    phonecast::vr::RadialMenuAction::ToggleVisible,
    phonecast::vr::RadialMenuAction::ShowGlance,
    phonecast::vr::RadialMenuAction::ShowPinned,
    phonecast::vr::RadialMenuAction::OpenSettings,
    phonecast::vr::RadialMenuAction::HeadLocked,
    phonecast::vr::RadialMenuAction::WorldLocked,
    phonecast::vr::RadialMenuAction::LeftControllerLocked,
    phonecast::vr::RadialMenuAction::RightControllerLocked};
constexpr std::array<const char*, 8> kDashboardLabels{
    "SHOW", "GLANCE", "PIN", "SETTINGS", "HEAD", "WORLD", "LEFT", "RIGHT"};

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

using Glyph = std::array<const char*, 7>;
const std::unordered_map<char, Glyph> kMenuGlyphs{
    {'A', {"01110", "10001", "10001", "11111", "10001", "10001", "10001"}},
    {'B', {"11110", "10001", "10001", "11110", "10001", "10001", "11110"}},
    {'C', {"01111", "10000", "10000", "10000", "10000", "10000", "01111"}},
    {'D', {"11110", "10001", "10001", "10001", "10001", "10001", "11110"}},
    {'E', {"11111", "10000", "10000", "11110", "10000", "10000", "11111"}},
    {'F', {"11111", "10000", "10000", "11110", "10000", "10000", "10000"}},
    {'G', {"01110", "10001", "10000", "10111", "10001", "10001", "01110"}},
    {'H', {"10001", "10001", "10001", "11111", "10001", "10001", "10001"}},
    {'I', {"11111", "00100", "00100", "00100", "00100", "00100", "11111"}},
    {'K', {"10001", "10010", "10100", "11000", "10100", "10010", "10001"}},
    {'L', {"10000", "10000", "10000", "10000", "10000", "10000", "11111"}},
    {'M', {"10001", "11011", "10101", "10101", "10001", "10001", "10001"}},
    {'N', {"10001", "11001", "10101", "10011", "10001", "10001", "10001"}},
    {'O', {"01110", "10001", "10001", "10001", "10001", "10001", "01110"}},
    {'P', {"11110", "10001", "10001", "11110", "10000", "10000", "10000"}},
    {'R', {"11110", "10001", "10001", "11110", "10100", "10010", "10001"}},
    {'S', {"01111", "10000", "10000", "01110", "00001", "00001", "11110"}},
    {'T', {"11111", "00100", "00100", "00100", "00100", "00100", "00100"}},
    {'U', {"10001", "10001", "10001", "10001", "10001", "10001", "01110"}},
    {'V', {"10001", "10001", "10001", "10001", "10001", "01010", "00100"}},
    {'W', {"10001", "10001", "10001", "10101", "10101", "10101", "01010"}},
    {'Y', {"10001", "10001", "01010", "00100", "00100", "00100", "00100"}},
    {'Z', {"11111", "00001", "00010", "00100", "01000", "10000", "11111"}},
    {'0', {"01110", "10001", "10011", "10101", "11001", "10001", "01110"}},
    {'1', {"00100", "01100", "00100", "00100", "00100", "00100", "01110"}},
    {'2', {"01110", "10001", "00001", "00010", "00100", "01000", "11111"}},
    {'3', {"11110", "00001", "00001", "01110", "00001", "00001", "11110"}},
    {'4', {"00010", "00110", "01010", "10010", "11111", "00010", "00010"}},
    {'5', {"11111", "10000", "10000", "11110", "00001", "00001", "11110"}},
    {'6', {"01110", "10000", "10000", "11110", "10001", "10001", "01110"}},
    {'7', {"11111", "00001", "00010", "00100", "01000", "01000", "01000"}},
    {'8', {"01110", "10001", "10001", "01110", "10001", "10001", "01110"}},
    {'9', {"01110", "10001", "10001", "01111", "00001", "00001", "01110"}},
    {'.', {"00000", "00000", "00000", "00000", "00000", "00110", "00110"}},
    {'-', {"00000", "00000", "00000", "11111", "00000", "00000", "00000"}},
    {'+', {"00000", "00100", "00100", "11111", "00100", "00100", "00000"}},
    {'>', {"10000", "01000", "00100", "00010", "00100", "01000", "10000"}}
};

void SetMenuPixel(std::vector<std::uint8_t>& image, int x, int y,
                  const std::array<std::uint8_t, 4>& color) {
    if (x < 0 || y < 0 || x >= static_cast<int>(kMenuTextureWidth) ||
        y >= static_cast<int>(kMenuTextureHeight)) return;
    const auto offset = (static_cast<std::size_t>(y) * kMenuTextureWidth +
                         static_cast<std::size_t>(x)) * 4U;
    std::copy(color.begin(), color.end(), image.begin() + static_cast<std::ptrdiff_t>(offset));
}

void DrawMenuLabel(std::vector<std::uint8_t>& image, const std::string& label,
                   int centerX, int centerY, int scale) {
    constexpr std::array<std::uint8_t, 4> color{245, 249, 255, 255};
    const int advance = 6 * scale;
    const int width = static_cast<int>(label.size()) * advance - scale;
    int cursorX = centerX - width / 2;
    const int originY = centerY - 7 * scale / 2;
    for (const char character : label) {
        const auto glyph = kMenuGlyphs.find(character);
        if (glyph != kMenuGlyphs.end()) {
            for (int row = 0; row < 7; ++row) {
                for (int column = 0; column < 5; ++column) {
                    if (glyph->second[static_cast<std::size_t>(row)][column] != '1') continue;
                    for (int yy = 0; yy < scale; ++yy)
                        for (int xx = 0; xx < scale; ++xx)
                            SetMenuPixel(image, cursorX + column * scale + xx,
                                         originY + row * scale + yy, color);
                }
            }
        }
        cursorX += advance;
    }
}

void SetSettingsPixel(std::vector<std::uint8_t>& image, int x, int y,
                      const std::array<std::uint8_t, 4>& color) {
    if (x < 0 || y < 0 || x >= static_cast<int>(kSettingsTextureWidth) ||
        y >= static_cast<int>(kSettingsTextureHeight)) return;
    const auto offset = (static_cast<std::size_t>(y) * kSettingsTextureWidth +
                         static_cast<std::size_t>(x)) * 4U;
    std::copy(color.begin(), color.end(), image.begin() + static_cast<std::ptrdiff_t>(offset));
}

void FillSettingsRect(std::vector<std::uint8_t>& image, int x, int y, int width, int height,
                      const std::array<std::uint8_t, 4>& color) {
    for (int row = y; row < y + height; ++row)
        for (int column = x; column < x + width; ++column)
            SetSettingsPixel(image, column, row, color);
}

void DrawSettingsLabel(std::vector<std::uint8_t>& image, const std::string& label,
                       int centerX, int centerY, int scale,
                       const std::array<std::uint8_t, 4>& color) {
    const int advance = 6 * scale;
    const int width = static_cast<int>(label.size()) * advance - scale;
    int cursorX = centerX - width / 2;
    const int originY = centerY - 7 * scale / 2;
    for (const char character : label) {
        const auto glyph = kMenuGlyphs.find(character);
        if (glyph != kMenuGlyphs.end()) {
            for (int row = 0; row < 7; ++row) {
                for (int column = 0; column < 5; ++column) {
                    if (glyph->second[static_cast<std::size_t>(row)][column] != '1') continue;
                    for (int yy = 0; yy < scale; ++yy)
                        for (int xx = 0; xx < scale; ++xx)
                            SetSettingsPixel(image, cursorX + column * scale + xx,
                                             originY + row * scale + yy, color);
                }
            }
        }
        cursorX += advance;
    }
}

std::vector<std::uint8_t> MakeSettingsTexture(const phonecast::vr::SettingsMenuView& view) {
    constexpr std::array<std::uint8_t, 4> background{8, 14, 24, 250};
    constexpr std::array<std::uint8_t, 4> border{42, 160, 235, 255};
    constexpr std::array<std::uint8_t, 4> selected{24, 92, 145, 255};
    constexpr std::array<std::uint8_t, 4> text{245, 249, 255, 255};
    constexpr std::array<std::uint8_t, 4> value{125, 218, 255, 255};
    std::vector<std::uint8_t> image(
        static_cast<std::size_t>(kSettingsTextureWidth) * kSettingsTextureHeight * 4U, 0);
    FillSettingsRect(image, 0, 0, static_cast<int>(kSettingsTextureWidth),
                     static_cast<int>(kSettingsTextureHeight), background);
    FillSettingsRect(image, 0, 0, static_cast<int>(kSettingsTextureWidth), 6, border);
    FillSettingsRect(image, 0, static_cast<int>(kSettingsTextureHeight) - 6,
                     static_cast<int>(kSettingsTextureWidth), 6, border);
    FillSettingsRect(image, 0, 0, 6, static_cast<int>(kSettingsTextureHeight), border);
    FillSettingsRect(image, static_cast<int>(kSettingsTextureWidth) - 6, 0, 6,
                     static_cast<int>(kSettingsTextureHeight), border);
    DrawSettingsLabel(image, view.title, static_cast<int>(kSettingsTextureWidth / 2), 52,
                      view.title.size() > 18 ? 2 : 3, text);
    const int rowHeight = 82;
    const int firstRow = 126;
    for (std::size_t index = 0; index < view.labels.size(); ++index) {
        const int centerY = firstRow + static_cast<int>(index) * rowHeight;
        if (index == view.selectedIndex)
            FillSettingsRect(image, 14, centerY - 34,
                             static_cast<int>(kSettingsTextureWidth) - 28, 68, selected);
        DrawSettingsLabel(image, view.labels[index], 190, centerY - 12,
                          view.labels[index].size() > 15 ? 1 : 2, text);
        const bool adjustable = index < view.values.size() &&
                                !view.values[index].empty() && view.values[index] != ">";
        if (index < view.values.size())
            DrawSettingsLabel(image, view.values[index], 354, centerY + 14,
                              view.values[index].size() > 13 ? 1 : 2, value);
        if (adjustable) {
            DrawSettingsLabel(image, "-", 42, centerY, 3, text);
            DrawSettingsLabel(image, "+", 470, centerY, 3, text);
        }
    }
    return image;
}

void SetGesturePixel(std::vector<std::uint8_t>& image, int x, int y,
                     const std::array<std::uint8_t, 4>& color) {
    if (x < 0 || y < 0 || x >= static_cast<int>(kGestureTextureSize) ||
        y >= static_cast<int>(kGestureTextureSize)) return;
    const auto offset = (static_cast<std::size_t>(y) * kGestureTextureSize +
                         static_cast<std::size_t>(x)) * 4U;
    std::copy(color.begin(), color.end(), image.begin() + static_cast<std::ptrdiff_t>(offset));
}

std::vector<std::uint8_t> MakeGestureProgressTexture(int completedSteps) {
    std::vector<std::uint8_t> image(
        static_cast<std::size_t>(kGestureTextureSize) * kGestureTextureSize * 4U, 0);
    const float center = static_cast<float>(kGestureTextureSize) * 0.5F;
    for (std::uint32_t y = 0; y < kGestureTextureSize; ++y) {
        for (std::uint32_t x = 0; x < kGestureTextureSize; ++x) {
            const float dx = static_cast<float>(x) - center;
            const float dy = center - static_cast<float>(y);
            const float radius = std::sqrt(dx * dx + dy * dy);
            if (radius < 43.0F || radius > 57.0F) continue;
            float angle = std::atan2(dx, dy);
            if (angle < 0.0F) angle += 2.0F * kPi;
            const int step = static_cast<int>(angle / (2.0F * kPi) * kGestureProgressSteps);
            const auto color = step < completedSteps
                ? std::array<std::uint8_t, 4>{35, 175, 245, 255}
                : std::array<std::uint8_t, 4>{70, 82, 98, 220};
            SetGesturePixel(image, static_cast<int>(x), static_cast<int>(y), color);
        }
    }
    return image;
}

void SetImagePixel(std::vector<std::uint8_t>& image, std::uint32_t width,
                   std::uint32_t height, int x, int y,
                   const std::array<std::uint8_t, 4>& color) {
    if (x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height)) return;
    const auto offset = (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4U;
    std::copy(color.begin(), color.end(), image.begin() + static_cast<std::ptrdiff_t>(offset));
}

void FillImageRect(std::vector<std::uint8_t>& image, std::uint32_t width,
                   std::uint32_t height, int x, int y, int rectWidth, int rectHeight,
                   const std::array<std::uint8_t, 4>& color) {
    for (int row = y; row < y + rectHeight; ++row)
        for (int column = x; column < x + rectWidth; ++column)
            SetImagePixel(image, width, height, column, row, color);
}

void DrawImageLabel(std::vector<std::uint8_t>& image, std::uint32_t width,
                    std::uint32_t height, const std::string& label,
                    int centerX, int centerY, int scale,
                    const std::array<std::uint8_t, 4>& color) {
    const int advance = 6 * scale;
    const int labelWidth = static_cast<int>(label.size()) * advance - scale;
    int cursorX = centerX - labelWidth / 2;
    const int originY = centerY - 7 * scale / 2;
    for (const char character : label) {
        const auto glyph = kMenuGlyphs.find(character);
        if (glyph != kMenuGlyphs.end()) {
            for (int row = 0; row < 7; ++row) {
                for (int column = 0; column < 5; ++column) {
                    if (glyph->second[static_cast<std::size_t>(row)][column] != '1') continue;
                    for (int yy = 0; yy < scale; ++yy)
                        for (int xx = 0; xx < scale; ++xx)
                            SetImagePixel(image, width, height,
                                          cursorX + column * scale + xx,
                                          originY + row * scale + yy, color);
                }
            }
        }
        cursorX += advance;
    }
}

std::vector<std::uint8_t> MakeDashboardTexture(bool currentlyVisible) {
    constexpr std::array<std::uint8_t, 4> background{8, 14, 24, 255};
    constexpr std::array<std::uint8_t, 4> header{15, 31, 50, 255};
    constexpr std::array<std::uint8_t, 4> cell{25, 52, 78, 255};
    constexpr std::array<std::uint8_t, 4> primary{22, 116, 184, 255};
    constexpr std::array<std::uint8_t, 4> text{245, 249, 255, 255};
    constexpr std::array<std::uint8_t, 4> accent{65, 188, 245, 255};
    constexpr int margin = 20;
    constexpr int gap = 12;
    constexpr int headerHeight = 104;
    const int cellWidth = (static_cast<int>(kDashboardTextureWidth) - margin * 2 -
                           gap * (kDashboardColumns - 1)) / kDashboardColumns;
    const int cellHeight = (static_cast<int>(kDashboardTextureHeight) - headerHeight -
                            margin - gap) / kDashboardRows;
    std::vector<std::uint8_t> image(
        static_cast<std::size_t>(kDashboardTextureWidth) * kDashboardTextureHeight * 4U, 0);
    FillImageRect(image, kDashboardTextureWidth, kDashboardTextureHeight, 0, 0,
                  static_cast<int>(kDashboardTextureWidth),
                  static_cast<int>(kDashboardTextureHeight), background);
    FillImageRect(image, kDashboardTextureWidth, kDashboardTextureHeight, 0, 0,
                  static_cast<int>(kDashboardTextureWidth), headerHeight, header);
    FillImageRect(image, kDashboardTextureWidth, kDashboardTextureHeight, 0,
                  headerHeight - 6, static_cast<int>(kDashboardTextureWidth), 6, accent);
    DrawImageLabel(image, kDashboardTextureWidth, kDashboardTextureHeight,
                   "PHONECAST", 190, 50, 4, text);
    DrawImageLabel(image, kDashboardTextureWidth, kDashboardTextureHeight,
                   currentlyVisible ? "PHONE VISIBLE" : "PHONE HIDDEN",
                   760, 50, 3, accent);
    for (int index = 0; index < kDashboardColumns * kDashboardRows; ++index) {
        const int column = index % kDashboardColumns;
        const int row = index / kDashboardColumns;
        const int left = margin + column * (cellWidth + gap);
        const int top = headerHeight + row * (cellHeight + gap);
        FillImageRect(image, kDashboardTextureWidth, kDashboardTextureHeight,
                      left, top, cellWidth, cellHeight, index == 0 ? primary : cell);
        const std::string label = index == 0 && currentlyVisible
            ? "HIDE" : kDashboardLabels[static_cast<std::size_t>(index)];
        DrawImageLabel(image, kDashboardTextureWidth, kDashboardTextureHeight,
                       label, left + cellWidth / 2, top + cellHeight / 2,
                       label.size() > 7 ? 3 : 4, text);
    }
    return image;
}

std::vector<std::uint8_t> MakeDashboardThumbnail() {
    constexpr std::array<std::uint8_t, 4> background{8, 24, 40, 255};
    constexpr std::array<std::uint8_t, 4> phone{31, 147, 220, 255};
    constexpr std::array<std::uint8_t, 4> screen{11, 18, 28, 255};
    constexpr std::array<std::uint8_t, 4> text{245, 249, 255, 255};
    std::vector<std::uint8_t> image(
        static_cast<std::size_t>(kDashboardThumbnailSize) * kDashboardThumbnailSize * 4U, 0);
    FillImageRect(image, kDashboardThumbnailSize, kDashboardThumbnailSize, 0, 0,
                  static_cast<int>(kDashboardThumbnailSize),
                  static_cast<int>(kDashboardThumbnailSize), background);
    FillImageRect(image, kDashboardThumbnailSize, kDashboardThumbnailSize,
                  63, 20, 130, 176, phone);
    FillImageRect(image, kDashboardThumbnailSize, kDashboardThumbnailSize,
                  73, 32, 110, 144, screen);
    DrawImageLabel(image, kDashboardThumbnailSize, kDashboardThumbnailSize,
                   "PC", 128, 104, 8, text);
    DrawImageLabel(image, kDashboardThumbnailSize, kDashboardThumbnailSize,
                   "PHONECAST", 128, 224, 3, text);
    return image;
}

int DashboardCell(float mouseX, float mouseY) {
    constexpr int margin = 20;
    constexpr int gap = 12;
    constexpr int headerHeight = 104;
    const int x = static_cast<int>(mouseX);
    const int y = static_cast<int>(kDashboardTextureHeight - mouseY);
    if (y < headerHeight) return -1;
    const int cellWidth = (static_cast<int>(kDashboardTextureWidth) - margin * 2 -
                           gap * (kDashboardColumns - 1)) / kDashboardColumns;
    const int cellHeight = (static_cast<int>(kDashboardTextureHeight) - headerHeight -
                            margin - gap) / kDashboardRows;
    for (int row = 0; row < kDashboardRows; ++row) {
        for (int column = 0; column < kDashboardColumns; ++column) {
            const int left = margin + column * (cellWidth + gap);
            const int top = headerHeight + row * (cellHeight + gap);
            if (x >= left && x < left + cellWidth && y >= top && y < top + cellHeight)
                return row * kDashboardColumns + column;
        }
    }
    return -1;
}

std::vector<std::uint8_t> MakeRadialMenuTexture(int selected, bool currentlyVisible) {
    constexpr int margin = 12;
    constexpr int gap = 8;
    const int cellWidth = (static_cast<int>(kMenuTextureWidth) - margin * 2 - gap) / kMenuColumns;
    const int cellHeight = (static_cast<int>(kMenuTextureHeight) - margin * 2 - gap * (kMenuRows - 1)) / kMenuRows;
    std::vector<std::uint8_t> image(
        static_cast<std::size_t>(kMenuTextureWidth) * kMenuTextureHeight * 4U, 0);
    for (std::uint32_t y = 0; y < kMenuTextureHeight; ++y)
        for (std::uint32_t x = 0; x < kMenuTextureWidth; ++x)
            SetMenuPixel(image, static_cast<int>(x), static_cast<int>(y), {8, 14, 24, 250});

    for (int index = 0; index < kMenuColumns * kMenuRows; ++index) {
        const int column = index % kMenuColumns;
        const int row = index / kMenuColumns;
        const int left = margin + column * (cellWidth + gap);
        const int top = margin + row * (cellHeight + gap);
        const bool populated = index < static_cast<int>(kMenuLabels.size());
        const std::array<std::uint8_t, 4> color = !populated
            ? std::array<std::uint8_t, 4>{12, 20, 32, 245}
            : index == selected
                ? std::array<std::uint8_t, 4>{28, 145, 235, 255}
                : std::array<std::uint8_t, 4>{25, 42, 64, 250};
        for (int y = top; y < top + cellHeight; ++y)
            for (int x = left; x < left + cellWidth; ++x)
                SetMenuPixel(image, x, y, color);
        if (!populated) continue;
        const std::string label = index == 0 && currentlyVisible ? "HIDE" : kMenuLabels[index];
        DrawMenuLabel(image, label, left + cellWidth / 2, top + cellHeight / 2,
                      label.size() > 6 ? 2 : 3);
    }
    return image;
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
#ifdef _WIN32
        const auto processId = static_cast<std::uint32_t>(GetCurrentProcessId());
#else
        const auto processId = static_cast<std::uint32_t>(getpid());
#endif
        const auto identifyResult = applications->IdentifyApplication(processId, kApplicationKey);
        if (identifyResult != vr::VRApplicationError_None) {
            const char* identifyName = applications->GetApplicationsErrorNameFromEnum(identifyResult);
            logger.Log(core::LogLevel::Warning, "openvr",
                       std::string("IdentifyApplication failed: ") +
                       (identifyName != nullptr ? identifyName : "unknown error"));
        } else {
            logger.Log(core::LogLevel::Info, "openvr", "Associated this process with the registered application key.");
        }
    }

    void RenderDashboard() {
        if (dashboardOverlay == vr::k_ulOverlayHandleInvalid) return;
        auto image = MakeDashboardTexture(desiredVisible);
        const auto result = overlayApi->SetOverlayRaw(
            dashboardOverlay, image.data(), kDashboardTextureWidth, kDashboardTextureHeight, 4);
        if (result != vr::VROverlayError_None)
            logger.Log(core::LogLevel::Warning, "openvr-dashboard",
                       "Failed to update the PhoneCast dashboard panel.");
    }

    void DestroyDashboard() {
        if (overlayApi == nullptr) return;
        if (dashboardOverlay != vr::k_ulOverlayHandleInvalid)
            overlayApi->DestroyOverlay(dashboardOverlay);
        if (dashboardThumbnail != vr::k_ulOverlayHandleInvalid)
            overlayApi->DestroyOverlay(dashboardThumbnail);
        dashboardOverlay = vr::k_ulOverlayHandleInvalid;
        dashboardThumbnail = vr::k_ulOverlayHandleInvalid;
    }

    void InitializeDashboard() {
        const auto result = overlayApi->CreateDashboardOverlay(
            kDashboardOverlayKey, kDashboardOverlayName,
            &dashboardOverlay, &dashboardThumbnail);
        if (result != vr::VROverlayError_None) {
            const char* name = overlayApi->GetOverlayErrorNameFromEnum(result);
            logger.Log(core::LogLevel::Warning, "openvr-dashboard",
                       std::string("Dashboard tab is unavailable: ") +
                       (name != nullptr ? name : "unknown error") + ".");
            dashboardOverlay = vr::k_ulOverlayHandleInvalid;
            dashboardThumbnail = vr::k_ulOverlayHandleInvalid;
            return;
        }

        auto thumbnail = MakeDashboardThumbnail();
        const vr::HmdVector2_t mouseScale{{static_cast<float>(kDashboardTextureWidth),
                                           static_cast<float>(kDashboardTextureHeight)}};
        const auto panelResult = overlayApi->SetOverlayRaw(
            dashboardOverlay, MakeDashboardTexture(desiredVisible).data(),
            kDashboardTextureWidth, kDashboardTextureHeight, 4);
        const auto thumbnailResult = overlayApi->SetOverlayRaw(
            dashboardThumbnail, thumbnail.data(),
            kDashboardThumbnailSize, kDashboardThumbnailSize, 4);
        const auto scaleResult = overlayApi->SetOverlayMouseScale(dashboardOverlay, &mouseScale);
        if (panelResult != vr::VROverlayError_None ||
            thumbnailResult != vr::VROverlayError_None ||
            scaleResult != vr::VROverlayError_None) {
            logger.Log(core::LogLevel::Warning, "openvr-dashboard",
                       "Dashboard tab was created but its panel, icon, or pointer scale failed.");
            DestroyDashboard();
            return;
        }
        logger.Log(core::LogLevel::Info, "openvr-dashboard",
                   "PhoneCast dashboard tab and launcher icon created.");
    }

    void QueueDashboardCell(int cell) {
        if (cell < 0 || cell >= static_cast<int>(kMenuActions.size())) return;
        pendingRadialMenuSelection.action = kMenuActions[static_cast<std::size_t>(cell)];
        pendingRadialMenuSelection.hand =
            currentSettings.placementMode == phonecast::vr::PlacementMode::RightControllerLocked
                ? phonecast::vr::GlanceInput::RightController
                : phonecast::vr::GlanceInput::LeftController;
        hasPendingRadialMenuSelection = true;
        logger.Log(core::LogLevel::Info, "openvr-dashboard",
                   "Dashboard control selected.");
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

    struct HandActions {
        vr::VRActionHandle_t glance{vr::k_ulInvalidActionHandle};
        vr::VRActionHandle_t calibrate{vr::k_ulInvalidActionHandle};
        vr::VRActionHandle_t grip{vr::k_ulInvalidActionHandle};
        vr::VRActionHandle_t trigger{vr::k_ulInvalidActionHandle};
        vr::VRActionHandle_t axis{vr::k_ulInvalidActionHandle};
    };

    struct HandInput {
        bool available{false};
        bool glanceDown{false};
        bool glancePressed{false};
        bool glanceReleased{false};
        bool calibratePressed{false};
        bool grip{false};
        bool trigger{false};
        float x{0.0F};
        float y{0.0F};
    };

    struct HoldState {
        bool tracking{false};
        std::chrono::steady_clock::time_point started{};
    };

    bool ResolveAction(const char* name, vr::VRActionHandle_t& handle) {
        const auto result = inputApi->GetActionHandle(name, &handle);
        if (result == vr::VRInputError_None) return true;
        logger.Log(core::LogLevel::Warning, "openvr-input",
                   std::string("GetActionHandle failed for ") + name + " (" +
                   std::to_string(static_cast<int>(result)) + ").");
        return false;
    }

    bool InitializeControllerInput() {
        inputApi = vr::VRInput();
        if (inputApi == nullptr) {
            logger.Log(core::LogLevel::Warning, "openvr-input", "IVRInput is unavailable; controller shortcuts are disabled.");
            return false;
        }
        std::error_code pathError;
        const auto executable = std::filesystem::absolute(executablePath, pathError);
        const auto manifest = executable.parent_path() / "phonecast-actions.json";
        if (pathError || !std::filesystem::exists(manifest)) {
            logger.Log(core::LogLevel::Warning, "openvr-input", "phonecast-actions.json was not found beside the executable.");
            return false;
        }
        auto result = inputApi->SetActionManifestPath(manifest.string().c_str());
        if (result != vr::VRInputError_None) {
            logger.Log(core::LogLevel::Warning, "openvr-input",
                       "SetActionManifestPath failed (" + std::to_string(static_cast<int>(result)) + ").");
            return false;
        }
        result = inputApi->GetActionSetHandle("/actions/phonecast", &actionSet);
        if (result != vr::VRInputError_None) {
            logger.Log(core::LogLevel::Warning, "openvr-input",
                       "GetActionSetHandle failed (" + std::to_string(static_cast<int>(result)) + ").");
            return false;
        }
        const bool resolved =
            ResolveAction("/actions/phonecast/in/glance_left", leftActions.glance) &&
            ResolveAction("/actions/phonecast/in/glance_right", rightActions.glance) &&
            ResolveAction("/actions/phonecast/in/calibrate_left", leftActions.calibrate) &&
            ResolveAction("/actions/phonecast/in/calibrate_right", rightActions.calibrate) &&
            ResolveAction("/actions/phonecast/in/grip_left", leftActions.grip) &&
            ResolveAction("/actions/phonecast/in/grip_right", rightActions.grip) &&
            ResolveAction("/actions/phonecast/in/trigger_left", leftActions.trigger) &&
            ResolveAction("/actions/phonecast/in/trigger_right", rightActions.trigger) &&
            ResolveAction("/actions/phonecast/in/axis_left", leftActions.axis) &&
            ResolveAction("/actions/phonecast/in/axis_right", rightActions.axis);
        if (!resolved) return false;
        logger.Log(core::LogLevel::Info, "openvr-input", "Explicit SteamVR Input actions initialized.");
        return true;
    }

    bool ReadDigital(vr::VRActionHandle_t action, bool& state, bool& rising,
                     bool& falling) const {
        vr::InputDigitalActionData_t data{};
        const auto result = inputApi->GetDigitalActionData(
            action, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
        if (result != vr::VRInputError_None || !data.bActive) return false;
        state = data.bState;
        rising = data.bChanged && data.bState;
        falling = data.bChanged && !data.bState;
        return true;
    }

    HandInput ReadHandInput(const HandActions& actions) const {
        HandInput input{};
        bool ignoredRising = false;
        bool ignoredFalling = false;
        bool calibrate = false;
        const bool haveGlance = ReadDigital(actions.glance, input.glanceDown,
                                            input.glancePressed, input.glanceReleased);
        const bool haveCalibrate = ReadDigital(actions.calibrate, calibrate,
                                               input.calibratePressed, ignoredFalling);
        const bool haveGrip = ReadDigital(actions.grip, input.grip, ignoredRising, ignoredFalling);
        const bool haveTrigger = ReadDigital(actions.trigger, input.trigger, ignoredRising, ignoredFalling);
        vr::InputAnalogActionData_t axis{};
        const auto axisResult = inputApi->GetAnalogActionData(
            actions.axis, &axis, sizeof(axis), vr::k_ulInvalidInputValueHandle);
        const bool haveAxis = axisResult == vr::VRInputError_None && axis.bActive;
        if (haveAxis) {
            input.x = axis.x;
            input.y = axis.y;
        }
        input.available = haveGlance || haveCalibrate || haveGrip || haveTrigger || haveAxis;
        return input;
    }

    bool PollExplicitInput(HandInput& left, HandInput& right) {
        vr::VRActiveActionSet_t active{};
        active.ulActionSet = actionSet;
        active.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle;
        // PhoneCast is an overlay rather than the focused scene application. A
        // normal-priority action set is therefore suppressed by the active game,
        // even though its manifest and bindings load successfully. Request the
        // runtime's overlay-global range so the configured PhoneCast controls can
        // be observed while a scene application owns input focus.
        active.nPriority = vr::k_nActionSetOverlayGlobalPriorityMin;
        const auto result = inputApi->UpdateActionState(&active, sizeof(active), 1);
        if (result != vr::VRInputError_None) {
            if (!actionUpdateErrorReported) {
                logger.Log(core::LogLevel::Warning, "openvr-input",
                           "UpdateActionState failed (" + std::to_string(static_cast<int>(result)) + ").");
                actionUpdateErrorReported = true;
            }
            return false;
        }
        left = ReadHandInput(leftActions);
        right = ReadHandInput(rightActions);
        return true;
    }

    void PollLegacyInput(HandInput& left, HandInput& right) {
        const auto read = [this](vr::ETrackedControllerRole role, std::uint64_t& previous) {
            HandInput input{};
            const auto device = system->GetTrackedDeviceIndexForControllerRole(role);
            vr::VRControllerState_t state{};
            if (device == vr::k_unTrackedDeviceIndexInvalid ||
                !system->GetControllerState(device, &state, sizeof(state))) return input;
            const auto oldButtons = previous;
            const auto rising = state.ulButtonPressed & ~oldButtons;
            const auto falling = oldButtons & ~state.ulButtonPressed;
            previous = state.ulButtonPressed;
            input.available = true;
            const auto padMask = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad);
            input.glanceDown = (state.ulButtonPressed & padMask) != 0;
            input.glancePressed = (rising & padMask) != 0;
            input.glanceReleased = (falling & padMask) != 0;
            input.calibratePressed = (rising & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu)) != 0;
            input.grip = (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip)) != 0;
            input.trigger = (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)) != 0;
            input.x = state.rAxis[0].x;
            input.y = state.rAxis[0].y;
            return input;
        };
        left = read(vr::TrackedControllerRole_LeftHand, previousLeftControllerButtons);
        right = read(vr::TrackedControllerRole_RightHand, previousRightControllerButtons);
    }

    bool CaptureMenuTransform() {
        vr::HmdMatrix34_t hmd{};
        if (!DevicePose(vr::k_unTrackedDeviceIndex_Hmd, hmd)) return false;
        const vr::HmdMatrix34_t local{{{1.0F, 0.0F, 0.0F, 0.0F},
                                       {0.0F, 1.0F, 0.0F, -0.04F},
                                       {0.0F, 0.0F, 1.0F, -0.85F}}};
        auto transform = Multiply(hmd, local);
        return overlayApi->SetOverlayTransformAbsolute(
            menuOverlay, vr::TrackingUniverseStanding, &transform) ==
            vr::VROverlayError_None;
    }

    bool WristFacesHead(bool left) const {
        vr::HmdMatrix34_t hmd{};
        vr::HmdMatrix34_t controller{};
        const auto mode = left ? phonecast::vr::PlacementMode::LeftControllerLocked
                               : phonecast::vr::PlacementMode::RightControllerLocked;
        if (!DevicePose(vr::k_unTrackedDeviceIndex_Hmd, hmd) ||
            !DevicePose(DeviceForMode(mode), controller)) return false;

        float toHead[3]{hmd.m[0][3] - controller.m[0][3],
                        hmd.m[1][3] - controller.m[1][3],
                        hmd.m[2][3] - controller.m[2][3]};
        const float distance = std::sqrt(toHead[0] * toHead[0] + toHead[1] * toHead[1] +
                                         toHead[2] * toHead[2]);
        const float vertical = controller.m[1][3] - hmd.m[1][3];
        if (distance < 0.18F || distance > 0.90F || vertical < -0.75F || vertical > 0.20F)
            return false;
        for (float& component : toHead) component /= distance;

        // OpenVR controller poses use local +Y as the top/control-face normal.
        // A high dot product means the user has turned that face toward the HMD.
        const float facing = controller.m[0][1] * toHead[0] +
                             controller.m[1][1] * toHead[1] +
                             controller.m[2][1] * toHead[2];
        return facing >= 0.62F;
    }

    void UpdateGestureProgress(float progress) {
        if (gestureOverlay == vr::k_ulOverlayHandleInvalid) return;
        if (progress <= 0.0F) {
            if (gestureProgressVisible) overlayApi->HideOverlay(gestureOverlay);
            gestureProgressVisible = false;
            gestureProgressStep = -1;
            return;
        }
        const int step = std::max(1, std::min(kGestureProgressSteps,
            static_cast<int>(std::ceil(progress * kGestureProgressSteps))));
        if (step != gestureProgressStep) {
            auto image = MakeGestureProgressTexture(step);
            if (overlayApi->SetOverlayRaw(gestureOverlay, image.data(),
                                          kGestureTextureSize, kGestureTextureSize, 4) ==
                vr::VROverlayError_None) {
                gestureProgressStep = step;
            }
        }
        if (!gestureProgressVisible) {
            overlayApi->ShowOverlay(gestureOverlay);
            gestureProgressVisible = true;
        }
    }

    bool HandleWristMenuGesture() {
        phonecast::vr::WristMenuGestureObservation observation;
        observation.leftFacing = WristFacesHead(true);
        observation.rightFacing = WristFacesHead(false);
        observation.menuVisible = radialMenuVisible;
        observation.nowMilliseconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());

        const auto result = wristMenuGesture.Update(observation);
        UpdateGestureProgress(result.progress);
        if (result.action == phonecast::vr::WristMenuGestureAction::OpenLeft ||
            result.action == phonecast::vr::WristMenuGestureAction::OpenRight) {
            UpdateGestureProgress(0.0F);
            const bool left = result.action == phonecast::vr::WristMenuGestureAction::OpenLeft;
            OpenRadialMenu(left, false);
            logger.Log(core::LogLevel::Info, "openvr-menu",
                       left ? "Wrist gesture opened the control grid for the left hand."
                            : "Wrist gesture opened the control grid for the right hand.");
            return true;
        }
        return false;
    }

    int MenuGridCell(float mouseX, float mouseY) const {
        constexpr int margin = 12;
        constexpr int gap = 8;
        const int cellWidth = (static_cast<int>(kMenuTextureWidth) - margin * 2 - gap) / kMenuColumns;
        const int cellHeight = (static_cast<int>(kMenuTextureHeight) - margin * 2 - gap * (kMenuRows - 1)) / kMenuRows;
        const int x = static_cast<int>(mouseX);
        const int y = static_cast<int>(kMenuTextureHeight - mouseY);
        for (int row = 0; row < kMenuRows; ++row) {
            for (int column = 0; column < kMenuColumns; ++column) {
                const int left = margin + column * (cellWidth + gap);
                const int top = margin + row * (cellHeight + gap);
                if (x >= left && x < left + cellWidth && y >= top && y < top + cellHeight) {
                    const int cell = row * kMenuColumns + column;
                    return cell < static_cast<int>(kMenuLabels.size()) ? cell : -1;
                }
            }
        }
        return -1;
    }

    void AcceptMenuCell(int cell) {
        if (cell == kCloseMenuCell) {
            CloseRadialMenu();
            logger.Log(core::LogLevel::Info, "openvr-menu", "Control grid closed.");
            return;
        }
        if (cell < 0 || cell >= static_cast<int>(kMenuActions.size())) return;
        pendingRadialMenuSelection.action = kMenuActions[static_cast<std::size_t>(cell)];
        pendingRadialMenuSelection.hand = radialMenuLeft
            ? phonecast::vr::GlanceInput::LeftController
            : phonecast::vr::GlanceInput::RightController;
        hasPendingRadialMenuSelection = true;
        CloseRadialMenu();
        logger.Log(core::LogLevel::Info, "openvr-menu",
                   "Control-grid selection accepted from the controller laser.");
    }

    void RenderRadialMenu() {
        if (menuOverlay == vr::k_ulOverlayHandleInvalid) return;
        auto image = MakeRadialMenuTexture(radialMenuSelection, desiredVisible);
        const auto result = overlayApi->SetOverlayRaw(
            menuOverlay, image.data(), kMenuTextureWidth, kMenuTextureHeight, 4);
        if (result != vr::VROverlayError_None)
            logger.Log(core::LogLevel::Warning, "openvr-menu", "Failed to update the control-grid texture.");
    }

    void OpenRadialMenu(bool left, bool waitForRelease) {
        if (menuOverlay == vr::k_ulOverlayHandleInvalid) return;
        radialMenuLeft = left;
        radialMenuSelection = -1;
        radialAwaitRelease = waitForRelease;
        if (!CaptureMenuTransform()) {
            logger.Log(core::LogLevel::Warning, "openvr-menu",
                       "Could not place the control grid in front of the headset.");
            return;
        }
        radialMenuVisible = true;
        RenderRadialMenu();
        overlayApi->ShowOverlay(menuOverlay);
        logger.Log(core::LogLevel::Info, "openvr-menu",
                   "Phone-shaped control grid opened in front of the headset.");
    }

    void CloseRadialMenu() {
        if (menuOverlay != vr::k_ulOverlayHandleInvalid) overlayApi->HideOverlay(menuOverlay);
        radialMenuVisible = false;
        radialAwaitRelease = false;
    }

    int SettingsRow(float mouseY) const {
        const int y = static_cast<int>(kSettingsTextureHeight - mouseY);
        constexpr int rowHeight = 82;
        constexpr int firstRow = 126;
        for (std::size_t row = 0; row < settingsMenuView.labels.size(); ++row) {
            const int center = firstRow + static_cast<int>(row) * rowHeight;
            if (y >= center - 34 && y < center + 34) return static_cast<int>(row);
        }
        return -1;
    }

    void QueueSettingsLaserClick(float mouseX, float mouseY) {
        const int row = SettingsRow(mouseY);
        if (row < 0) return;
        settingsLaserTargetRow = row;
        const bool adjustable = static_cast<std::size_t>(row) < settingsMenuView.values.size() &&
                                !settingsMenuView.values[static_cast<std::size_t>(row)].empty() &&
                                settingsMenuView.values[static_cast<std::size_t>(row)] != ">";
        if (adjustable && mouseX < 100.0F)
            settingsLaserCommand = phonecast::vr::SettingsMenuCommand::Decrease;
        else if (adjustable && mouseX > static_cast<float>(kSettingsTextureWidth) - 100.0F)
            settingsLaserCommand = phonecast::vr::SettingsMenuCommand::Increase;
        else
            settingsLaserCommand = phonecast::vr::SettingsMenuCommand::Activate;
    }

    bool HandleSettingsMenu(const HandInput& left, const HandInput& right) {
        if (!settingsMenuVisible) return false;
        const auto& active = settingsMenuLeft ? left : right;
        if (!hasPendingSettingsMenuCommand &&
            (left.calibratePressed || right.calibratePressed)) {
            pendingSettingsMenuCommand = phonecast::vr::SettingsMenuCommand::Back;
            hasPendingSettingsMenuCommand = true;
        } else if (!hasPendingSettingsMenuCommand && active.glancePressed) {
            pendingSettingsMenuCommand = phonecast::vr::SettingsMenuCommand::Activate;
            hasPendingSettingsMenuCommand = true;
        }
        const float magnitude = std::sqrt(active.x * active.x + active.y * active.y);
        if (magnitude < 0.30F) {
            settingsAxisLatched = false;
        } else if (!settingsAxisLatched && !hasPendingSettingsMenuCommand) {
            if (std::fabs(active.y) >= std::fabs(active.x))
                pendingSettingsMenuCommand = active.y > 0.0F
                    ? phonecast::vr::SettingsMenuCommand::PreviousItem
                    : phonecast::vr::SettingsMenuCommand::NextItem;
            else
                pendingSettingsMenuCommand = active.x > 0.0F
                    ? phonecast::vr::SettingsMenuCommand::Increase
                    : phonecast::vr::SettingsMenuCommand::Decrease;
            hasPendingSettingsMenuCommand = true;
            settingsAxisLatched = true;
        }
        return true;
    }

    bool HandleRadialMenu(const HandInput& left, const HandInput& right) {
        if (!radialMenuVisible) return false;
        const auto& active = radialMenuLeft ? left : right;
        if (active.glanceReleased) radialAwaitRelease = false;
        if (left.calibratePressed || right.calibratePressed) {
            CloseRadialMenu();
            logger.Log(core::LogLevel::Info, "openvr-menu", "Control grid cancelled.");
        } else if (!radialAwaitRelease && active.glancePressed && radialMenuSelection >= 0) {
            AcceptMenuCell(radialMenuSelection);
        }
        return true;
    }

    bool HandleGlancePress(bool left, const HandInput& input, HoldState& hold) {
        if (input.glancePressed) {
            hold.tracking = true;
            hold.started = std::chrono::steady_clock::now();
        }
        if (hold.tracking && input.glanceDown &&
            std::chrono::steady_clock::now() - hold.started >=
                std::chrono::milliseconds(currentSettings.radialLongPressMilliseconds)) {
            hold.tracking = false;
            OpenRadialMenu(left, true);
            return true;
        }
        if (hold.tracking && input.glanceReleased) {
            hold.tracking = false;
            pendingGlanceInput = left
                ? phonecast::vr::GlanceInput::LeftController
                : phonecast::vr::GlanceInput::RightController;
            hasPendingGlanceInput = true;
        }
        return false;
    }

    void PollControllerCalibration() {
        HandInput left{};
        HandInput right{};
        if (!explicitInputReady || !PollExplicitInput(left, right)) PollLegacyInput(left, right);

        if (HandleSettingsMenu(left, right)) return;
        if (kEnableExperimentalGestureControls &&
            !settingsMenuVisible && HandleWristMenuGesture()) return;
        if (HandleRadialMenu(left, right)) return;
        if (kEnableExperimentalGestureControls && !calibrationActive) {
            if (HandleGlancePress(true, left, leftHold)) return;
            if (HandleGlancePress(false, right, rightHold)) return;
        }
        const bool selectLeft = left.calibratePressed &&
            currentSettings.placementMode != phonecast::vr::PlacementMode::LeftControllerLocked;
        const bool selectRight = right.calibratePressed &&
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
        const auto& state = leftSelected ? left : right;
        if (!state.available) return;

        if (state.calibratePressed) {
            calibrationActive = !calibrationActive;
            if (!calibrationActive && calibrationDirty) PublishCalibration();
            logger.Log(core::LogLevel::Info, "openvr", calibrationActive
                ? "Controller calibration enabled: axis moves; grip+axis rotates; trigger+axis scales/distances; pad click changes orientation."
                : "Controller calibration disabled and saved.");
        }
        if (!calibrationActive) return;

        auto& calibration = ControllerFor(currentSettings);
        if (state.glancePressed) {
            if (state.grip) {
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
        float x = state.x;
        float y = state.y;
        if (std::fabs(x) < 0.18F) x = 0.0F;
        if (std::fabs(y) < 0.18F) y = 0.0F;
        if (x == 0.0F && y == 0.0F) {
            if (calibrationDirty && !state.grip && !state.trigger && !state.glancePressed)
                PublishCalibration();
            return;
        }
        if (state.grip) {
            calibration.yawDegrees = Clamp(calibration.yawDegrees + x * 2.0F, -180.0F, 180.0F);
            calibration.tiltDegrees = Clamp(calibration.tiltDegrees + y * 2.0F, -180.0F, 180.0F);
        } else if (state.trigger) {
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
    vr::IVRInput* inputApi{nullptr};
    vr::VRActionSetHandle_t actionSet{vr::k_ulInvalidActionSetHandle};
    HandActions leftActions{};
    HandActions rightActions{};
    bool explicitInputReady{false};
    bool actionUpdateErrorReported{false};
    vr::VROverlayHandle_t overlay{vr::k_ulOverlayHandleInvalid};
    vr::VROverlayHandle_t menuOverlay{vr::k_ulOverlayHandleInvalid};
    vr::VROverlayHandle_t gestureOverlay{vr::k_ulOverlayHandleInvalid};
    vr::VROverlayHandle_t settingsOverlay{vr::k_ulOverlayHandleInvalid};
    vr::VROverlayHandle_t dashboardOverlay{vr::k_ulOverlayHandleInvalid};
    vr::VROverlayHandle_t dashboardThumbnail{vr::k_ulOverlayHandleInvalid};
    bool gestureProgressVisible{false};
    int gestureProgressStep{-1};
    bool settingsMenuVisible{false};
    bool settingsMenuLeft{true};
    phonecast::vr::SettingsMenuView settingsMenuView{};
    int settingsLaserTargetRow{-1};
    phonecast::vr::SettingsMenuCommand settingsLaserCommand{
        phonecast::vr::SettingsMenuCommand::Activate};
    bool settingsAxisLatched{false};
    phonecast::vr::SettingsMenuCommand pendingSettingsMenuCommand{
        phonecast::vr::SettingsMenuCommand::Back};
    bool hasPendingSettingsMenuCommand{false};
    bool radialMenuVisible{false};
    bool radialMenuLeft{true};
    bool radialAwaitRelease{false};
    int radialMenuSelection{-1};
    phonecast::vr::RadialMenuSelection pendingRadialMenuSelection{};
    bool hasPendingRadialMenuSelection{false};
    HoldState leftHold{};
    HoldState rightHold{};
    phonecast::vr::WristMenuGesture wristMenuGesture{};
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
    impl_->explicitInputReady = impl_->InitializeControllerInput();
    impl_->overlayApi = vr::VROverlay();
    if (impl_->overlayApi == nullptr) {
        error = "OpenVR did not provide IVROverlay.";
        Stop();
        return false;
    }
    if (!impl_->OverlayCall(impl_->overlayApi->CreateOverlay(kOverlayKey, kOverlayName, &impl_->overlay),
                            "CreateOverlay", error) ||
        !impl_->OverlayCall(impl_->overlayApi->CreateOverlay(
                                kMenuOverlayKey, kMenuOverlayName, &impl_->menuOverlay),
                            "Create control-grid overlay", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayWidthInMeters(impl_->menuOverlay, 0.34F),
                            "Set control-grid width", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlaySortOrder(impl_->menuOverlay, 100U),
                            "Set control-grid sort order", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayInputMethod(
                                impl_->menuOverlay, vr::VROverlayInputMethod_Mouse),
                            "Set control-grid input method", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayFlag(
                                impl_->menuOverlay,
                                vr::VROverlayFlags_MakeOverlaysInteractiveIfVisible, true),
                            "Set control-grid interactive flag", error) ||
        !impl_->OverlayCall(impl_->overlayApi->CreateOverlay(
                                kGestureOverlayKey, kGestureOverlayName, &impl_->gestureOverlay),
                            "Create gesture-progress overlay", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayWidthInMeters(
                                impl_->gestureOverlay, 0.08F),
                            "Set gesture-progress width", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlaySortOrder(
                                impl_->gestureOverlay, 102U),
                            "Set gesture-progress sort order", error) ||
        !impl_->OverlayCall(impl_->overlayApi->CreateOverlay(
                                kSettingsOverlayKey, kSettingsOverlayName, &impl_->settingsOverlay),
                            "Create settings overlay", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayWidthInMeters(impl_->settingsOverlay, 0.34F),
                            "Set settings width", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlaySortOrder(impl_->settingsOverlay, 101U),
                            "Set settings sort order", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayInputMethod(
                                impl_->settingsOverlay, vr::VROverlayInputMethod_Mouse),
                            "Set settings input method", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayFlag(
                                impl_->settingsOverlay,
                                vr::VROverlayFlags_MakeOverlaysInteractiveIfVisible, true),
                            "Set settings interactive flag", error)) {
        Stop();
        return false;
    }

    const vr::HmdVector2_t menuMouseScale{{static_cast<float>(kMenuTextureWidth),
                                           static_cast<float>(kMenuTextureHeight)}};
    if (!impl_->OverlayCall(impl_->overlayApi->SetOverlayMouseScale(
                                impl_->menuOverlay, &menuMouseScale),
                            "Set control-grid mouse scale", error)) {
        Stop();
        return false;
    }

    const vr::HmdVector2_t settingsMouseScale{{static_cast<float>(kSettingsTextureWidth),
                                               static_cast<float>(kSettingsTextureHeight)}};
    auto settingsTransform = vr::HmdMatrix34_t{{{1.0F, 0.0F, 0.0F, 0.0F},
                                                 {0.0F, 1.0F, 0.0F, -0.04F},
                                                 {0.0F, 0.0F, 1.0F, -0.85F}}};
    auto gestureTransform = vr::HmdMatrix34_t{{{1.0F, 0.0F, 0.0F, 0.0F},
                                                {0.0F, 1.0F, 0.0F, -0.18F},
                                                {0.0F, 0.0F, 1.0F, -0.60F}}};
    if (!impl_->OverlayCall(impl_->overlayApi->SetOverlayTransformTrackedDeviceRelative(
                                impl_->settingsOverlay, vr::k_unTrackedDeviceIndex_Hmd,
                                &settingsTransform),
                            "Set settings transform", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayMouseScale(
                                impl_->settingsOverlay, &settingsMouseScale),
                            "Set settings mouse scale", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayTransformTrackedDeviceRelative(
                                impl_->gestureOverlay, vr::k_unTrackedDeviceIndex_Hmd,
                                &gestureTransform),
                            "Set gesture-progress transform", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayInputMethod(
                                impl_->overlay, vr::VROverlayInputMethod_Mouse),
                            "SetOverlayInputMethod", error) ||
        !impl_->OverlayCall(impl_->overlayApi->SetOverlayFlag(
                                impl_->overlay, vr::VROverlayFlags_MakeOverlaysInteractiveIfVisible, true),
                            "SetOverlayFlag", error) ||
        !impl_->ApplySettings(settings, error)) {
        Stop();
        return false;
    }
    impl_->InitializeDashboard();
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
    const bool visibilityChanged = impl_->desiredVisible != visible;
    impl_->desiredVisible = visible;
    if (visibilityChanged) impl_->RenderDashboard();
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
    while (impl_->dashboardOverlay != vr::k_ulOverlayHandleInvalid &&
           impl_->overlayApi->PollNextOverlayEvent(
               impl_->dashboardOverlay, &event, sizeof(event))) {
        if (event.eventType == vr::VREvent_MouseButtonDown &&
            (event.data.mouse.button & vr::VRMouseButton_Left) != 0) {
            impl_->QueueDashboardCell(
                DashboardCell(event.data.mouse.x, event.data.mouse.y));
        }
        if (IsQuitEvent(event.eventType)) {
            impl_->logger.Log(core::LogLevel::Info, "openvr",
                              "Runtime requested overlay shutdown.");
            return false;
        }
    }
    while (impl_->overlayApi->PollNextOverlayEvent(
               impl_->settingsOverlay, &event, sizeof(event))) {
        if (event.eventType == vr::VREvent_MouseButtonDown &&
            (event.data.mouse.button & vr::VRMouseButton_Left) != 0) {
            impl_->QueueSettingsLaserClick(event.data.mouse.x, event.data.mouse.y);
        }
        if (IsQuitEvent(event.eventType)) {
            impl_->logger.Log(core::LogLevel::Info, "openvr", "Runtime requested overlay shutdown.");
            return false;
        }
    }
    while (impl_->overlayApi->PollNextOverlayEvent(
               impl_->menuOverlay, &event, sizeof(event))) {
        if (event.eventType == vr::VREvent_MouseMove) {
            // Keep the static grid texture stable; the runtime laser supplies
            // hover feedback without repeated SetOverlayRaw submissions.
            impl_->radialMenuSelection = impl_->MenuGridCell(
                event.data.mouse.x, event.data.mouse.y);
        } else if (event.eventType == vr::VREvent_MouseButtonDown &&
                   (event.data.mouse.button & vr::VRMouseButton_Left) != 0) {
            impl_->AcceptMenuCell(
                impl_->MenuGridCell(event.data.mouse.x, event.data.mouse.y));
        }
        if (IsQuitEvent(event.eventType)) {
            impl_->logger.Log(core::LogLevel::Info, "openvr", "Runtime requested overlay shutdown.");
            return false;
        }
    }
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

bool OpenVrOverlayRenderer::TakeRadialMenuSelection(
        phonecast::vr::RadialMenuSelection& selection) {
    if (!impl_->hasPendingRadialMenuSelection) return false;
    selection = impl_->pendingRadialMenuSelection;
    impl_->hasPendingRadialMenuSelection = false;
    return true;
}

bool OpenVrOverlayRenderer::ShowSettingsMenu(
        const phonecast::vr::SettingsMenuView& view, std::string& error) {
    if (impl_->overlayApi == nullptr ||
        impl_->settingsOverlay == vr::k_ulOverlayHandleInvalid) {
        error = "The OpenVR settings overlay is unavailable.";
        return false;
    }
    impl_->settingsMenuView = view;
    auto image = MakeSettingsTexture(view);
    if (!impl_->OverlayCall(impl_->overlayApi->SetOverlayRaw(
                                impl_->settingsOverlay, image.data(),
                                kSettingsTextureWidth, kSettingsTextureHeight, 4),
                            "Set settings texture", error) ||
        !impl_->OverlayCall(impl_->overlayApi->ShowOverlay(impl_->settingsOverlay),
                            "Show settings overlay", error)) return false;
    impl_->settingsMenuLeft = impl_->radialMenuLeft;
    impl_->settingsMenuVisible = true;
    impl_->logger.Log(core::LogLevel::Info, "openvr-settings", "In-headset settings menu shown.");
    error.clear();
    return true;
}

bool OpenVrOverlayRenderer::HideSettingsMenu(std::string& error) {
    if (impl_->overlayApi == nullptr ||
        impl_->settingsOverlay == vr::k_ulOverlayHandleInvalid) {
        error = "The OpenVR settings overlay is unavailable.";
        return false;
    }
    if (!impl_->OverlayCall(impl_->overlayApi->HideOverlay(impl_->settingsOverlay),
                            "Hide settings overlay", error)) return false;
    impl_->settingsMenuVisible = false;
    impl_->settingsAxisLatched = false;
    impl_->hasPendingSettingsMenuCommand = false;
    impl_->settingsLaserTargetRow = -1;
    impl_->logger.Log(core::LogLevel::Info, "openvr-settings", "In-headset settings menu hidden.");
    error.clear();
    return true;
}

bool OpenVrOverlayRenderer::TakeSettingsMenuInput(
        phonecast::vr::SettingsMenuCommand& command) {
    if (impl_->hasPendingSettingsMenuCommand) {
        command = impl_->pendingSettingsMenuCommand;
        impl_->hasPendingSettingsMenuCommand = false;
        return true;
    }
    if (impl_->settingsLaserTargetRow < 0) return false;
    if (impl_->settingsMenuView.selectedIndex !=
        static_cast<std::size_t>(impl_->settingsLaserTargetRow)) {
        const auto count = impl_->settingsMenuView.labels.size();
        const auto current = impl_->settingsMenuView.selectedIndex;
        const auto target = static_cast<std::size_t>(impl_->settingsLaserTargetRow);
        const auto forward = (target + count - current) % count;
        const auto backward = (current + count - target) % count;
        command = forward <= backward
            ? phonecast::vr::SettingsMenuCommand::NextItem
            : phonecast::vr::SettingsMenuCommand::PreviousItem;
        return true;
    }
    command = impl_->settingsLaserCommand;
    impl_->settingsLaserTargetRow = -1;
    return true;
}

void OpenVrOverlayRenderer::Stop() noexcept {
    impl_->DestroyDashboard();
    if (impl_->overlayApi != nullptr && impl_->gestureOverlay != vr::k_ulOverlayHandleInvalid) {
        impl_->overlayApi->HideOverlay(impl_->gestureOverlay);
        impl_->overlayApi->DestroyOverlay(impl_->gestureOverlay);
    }
    if (impl_->overlayApi != nullptr && impl_->settingsOverlay != vr::k_ulOverlayHandleInvalid) {
        impl_->overlayApi->HideOverlay(impl_->settingsOverlay);
        impl_->overlayApi->DestroyOverlay(impl_->settingsOverlay);
    }
    if (impl_->overlayApi != nullptr && impl_->menuOverlay != vr::k_ulOverlayHandleInvalid) {
        impl_->overlayApi->HideOverlay(impl_->menuOverlay);
        impl_->overlayApi->DestroyOverlay(impl_->menuOverlay);
    }
    if (impl_->overlayApi != nullptr && impl_->overlay != vr::k_ulOverlayHandleInvalid) {
        impl_->overlayApi->HideOverlay(impl_->overlay);
        impl_->overlayApi->DestroyOverlay(impl_->overlay);
    }
    impl_->gestureOverlay = vr::k_ulOverlayHandleInvalid;
    impl_->settingsOverlay = vr::k_ulOverlayHandleInvalid;
    impl_->dashboardOverlay = vr::k_ulOverlayHandleInvalid;
    impl_->dashboardThumbnail = vr::k_ulOverlayHandleInvalid;
    impl_->menuOverlay = vr::k_ulOverlayHandleInvalid;
    impl_->overlay = vr::k_ulOverlayHandleInvalid;
    impl_->overlayApi = nullptr;
    impl_->shown = false;
    impl_->desiredVisible = true;
    impl_->hasFrame = false;
    impl_->hasPendingSettings = false;
    impl_->hasPendingGlanceInput = false;
    impl_->hasPendingRadialMenuSelection = false;
    impl_->hasPendingSettingsMenuCommand = false;
    impl_->gestureProgressVisible = false;
    impl_->gestureProgressStep = -1;
    impl_->settingsMenuVisible = false;
    impl_->settingsAxisLatched = false;
    impl_->settingsLaserTargetRow = -1;
    impl_->radialMenuVisible = false;
    impl_->radialAwaitRelease = false;
    impl_->radialMenuSelection = -1;
    impl_->leftHold = {};
    impl_->rightHold = {};
    impl_->wristMenuGesture.Reset();
    impl_->explicitInputReady = false;
    impl_->actionUpdateErrorReported = false;
    impl_->inputApi = nullptr;
    impl_->actionSet = vr::k_ulInvalidActionSetHandle;
    impl_->leftActions = {};
    impl_->rightActions = {};
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
