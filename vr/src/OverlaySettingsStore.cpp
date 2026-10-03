#include "phonecast/vr/overlay/OverlaySettingsStore.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace phonecast::vr {
namespace {

const char* ModeName(PlacementMode mode) {
    switch (mode) {
        case PlacementMode::HeadLocked: return "head";
        case PlacementMode::WorldLocked: return "world";
        case PlacementMode::LeftControllerLocked: return "left-controller";
        case PlacementMode::RightControllerLocked: return "right-controller";
    }
    return "head";
}

bool ParseMode(const std::string& text, PlacementMode& mode) {
    if (text == "head") mode = PlacementMode::HeadLocked;
    else if (text == "world") mode = PlacementMode::WorldLocked;
    else if (text == "left-controller") mode = PlacementMode::LeftControllerLocked;
    else if (text == "right-controller") mode = PlacementMode::RightControllerLocked;
    else return false;
    return true;
}

bool ParseFloat(const std::string& text, float minimum, float maximum, float& value) {
    try {
        std::size_t end = 0;
        const float parsed = std::stof(text, &end);
        if (end != text.size() || !std::isfinite(parsed) || parsed < minimum || parsed > maximum)
            return false;
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

OverlaySettingsStore::OverlaySettingsStore(std::filesystem::path path) : path_(std::move(path)) {}

bool OverlaySettingsStore::Load(OverlaySettings& settings, bool& found, std::string& error) const {
    found = false;
    error.clear();
    std::ifstream input(path_);
    if (!input) {
        if (!std::filesystem::exists(path_)) return true;
        error = "Could not open overlay settings: " + path_.string();
        return false;
    }
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line.front() == '#') continue;
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            error = "Invalid overlay settings line: " + line;
            return false;
        }
        values[line.substr(0, separator)] = line.substr(separator + 1);
    }
    if (values["version"] != "1") {
        error = "Unsupported overlay settings version.";
        return false;
    }

    OverlaySettings loaded;
    if (!ParseMode(values["mode"], loaded.placementMode) ||
        !ParseFloat(values["width"], 0.1F, 5.0F, loaded.widthMeters) ||
        !ParseFloat(values["distance"], 0.2F, 10.0F, loaded.distanceMeters) ||
        !ParseFloat(values["controller_distance"], 0.05F, 2.0F,
                    loaded.controllerDistanceMeters) ||
        !ParseFloat(values["alpha"], 0.0F, 1.0F, loaded.alpha) ||
        !ParseFloat(values["offset_x"], -3.0F, 3.0F, loaded.offsetXMeters) ||
        !ParseFloat(values["offset_y"], -3.0F, 3.0F, loaded.offsetYMeters)) {
        error = "Overlay settings contain an invalid value.";
        return false;
    }
    loaded.worldTransformValid = values["world_valid"] == "1";
    for (std::size_t index = 0; index < loaded.worldTransform.size(); ++index) {
        if (!ParseFloat(values["world_" + std::to_string(index)], -10000.0F, 10000.0F,
                        loaded.worldTransform[index])) {
            error = "Overlay settings contain an invalid world transform.";
            return false;
        }
    }
    settings = loaded;
    found = true;
    return true;
}

bool OverlaySettingsStore::Save(const OverlaySettings& settings, std::string& error) const {
    error.clear();
    std::error_code filesystemError;
    if (path_.has_parent_path())
        std::filesystem::create_directories(path_.parent_path(), filesystemError);
    if (filesystemError) {
        error = "Could not create the overlay settings directory: " + filesystemError.message();
        return false;
    }
    const auto temporary = path_.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) {
        error = "Could not write overlay settings: " + temporary;
        return false;
    }
    output << std::setprecision(9)
           << "version=1\n"
           << "mode=" << ModeName(settings.placementMode) << '\n'
           << "width=" << settings.widthMeters << '\n'
           << "distance=" << settings.distanceMeters << '\n'
           << "controller_distance=" << settings.controllerDistanceMeters << '\n'
           << "alpha=" << settings.alpha << '\n'
           << "offset_x=" << settings.offsetXMeters << '\n'
           << "offset_y=" << settings.offsetYMeters << '\n'
           << "world_valid=" << (settings.worldTransformValid ? 1 : 0) << '\n';
    for (std::size_t index = 0; index < settings.worldTransform.size(); ++index)
        output << "world_" << index << '=' << settings.worldTransform[index] << '\n';
    output.close();
    if (!output) {
        error = "Could not finish writing overlay settings.";
        std::filesystem::remove(temporary, filesystemError);
        return false;
    }
    std::filesystem::remove(path_, filesystemError);
    filesystemError.clear();
    std::filesystem::rename(temporary, path_, filesystemError);
    if (filesystemError) {
        error = "Could not replace overlay settings: " + filesystemError.message();
        return false;
    }
    return true;
}

}  // namespace phonecast::vr
