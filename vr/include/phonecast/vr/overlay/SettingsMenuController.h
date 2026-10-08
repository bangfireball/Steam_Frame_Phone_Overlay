#pragma once

#include "phonecast/vr/overlay/IOverlayRenderer.h"
#include <utility>

namespace phonecast::vr {

enum class SettingsMenuResult {
    None,
    Updated,
    Applied,
    Cancelled,
    ExportRequested
};

class SettingsMenuController {
public:
    void Open(const OverlaySettings& settings);
    [[nodiscard]] bool IsOpen() const noexcept { return open_; }
    [[nodiscard]] const OverlaySettings& Draft() const noexcept { return draft_; }
    [[nodiscard]] const OverlaySettings& Original() const noexcept { return original_; }
    [[nodiscard]] SettingsMenuView View() const;
    void MergeRendererUpdate(const OverlaySettings& settings) noexcept;
    void SetAudioStatus(std::string status) { audioStatus_ = std::move(status); }
    void SetExportStatus(std::string status, std::string filename = {}) {
        exportStatus_ = std::move(status); exportFilename_ = std::move(filename);
    }
    SettingsMenuResult Handle(SettingsMenuCommand command);
    SettingsMenuResult Handle(const SettingsMenuInput& input);
    void Close() noexcept { open_ = false; }

private:
    enum class Page {
        Root,
        Appearance,
        Placement,
        LeftController,
        RightController,
        Glance,
        Notifications,
        Audio,
        StartLocation,
        About
    };

    [[nodiscard]] std::size_t ItemCount() const noexcept;
    SettingsMenuResult Activate();
    SettingsMenuResult Adjust(int direction);
    SettingsMenuResult SetNormalized(float value);

    bool open_{false};
    bool resetConfirmation_{false};
    Page page_{Page::Root};
    std::size_t selected_{0};
    std::string audioStatus_{"OFF"};
    std::string exportStatus_{"REVIEW BEFORE SHARING"};
    std::string exportFilename_{"NONE"};
    bool exportConfirmation_{false};
    OverlaySettings original_{};
    OverlaySettings draft_{};
};

}  // namespace phonecast::vr
