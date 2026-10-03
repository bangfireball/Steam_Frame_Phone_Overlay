#pragma once

#include "phonecast/vr/overlay/IOverlayRenderer.h"

#include <filesystem>
#include <string>

namespace phonecast::vr {

class OverlaySettingsStore {
public:
    explicit OverlaySettingsStore(std::filesystem::path path);

    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path_; }
    bool Load(OverlaySettings& settings, bool& found, std::string& error) const;
    bool Save(const OverlaySettings& settings, std::string& error) const;

private:
    std::filesystem::path path_;
};

}  // namespace phonecast::vr
