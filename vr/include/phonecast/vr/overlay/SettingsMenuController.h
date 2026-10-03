#pragma once

#include "phonecast/vr/overlay/IOverlayRenderer.h"

namespace phonecast::vr {

enum class SettingsMenuResult {
    None,
    Updated,
    Applied,
    Cancelled
};

class SettingsMenuController {
public:
    void Open(const OverlaySettings& settings);
    [[nodiscard]] bool IsOpen() const noexcept { return open_; }
    [[nodiscard]] const OverlaySettings& Draft() const noexcept { return draft_; }
    [[nodiscard]] const OverlaySettings& Original() const noexcept { return original_; }
    [[nodiscard]] SettingsMenuView View() const;
    void MergeRendererUpdate(const OverlaySettings& settings) noexcept;
    SettingsMenuResult Handle(SettingsMenuCommand command);
    void Close() noexcept { open_ = false; }

private:
    enum class Page {
        Root,
        Appearance,
        Placement,
        LeftController,
        RightController,
        Glance
    };

    [[nodiscard]] std::size_t ItemCount() const noexcept;
    SettingsMenuResult Activate();
    SettingsMenuResult Adjust(int direction);

    bool open_{false};
    bool resetConfirmation_{false};
    Page page_{Page::Root};
    std::size_t selected_{0};
    OverlaySettings original_{};
    OverlaySettings draft_{};
};

}  // namespace phonecast::vr
