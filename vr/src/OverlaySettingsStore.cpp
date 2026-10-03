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

const char* OrientationName(ControllerOrientation orientation) {
    switch (orientation) {
        case ControllerOrientation::ControllerRelative: return "controller-relative";
        case ControllerOrientation::FaceUser: return "face-user";
        case ControllerOrientation::WorldUpright: return "world-upright";
        case ControllerOrientation::Wrist: return "wrist";
    }
    return "face-user";
}

bool ParseOrientation(const std::string& text, ControllerOrientation& orientation) {
    if (text == "controller-relative") orientation = ControllerOrientation::ControllerRelative;
    else if (text == "face-user") orientation = ControllerOrientation::FaceUser;
    else if (text == "world-upright") orientation = ControllerOrientation::WorldUpright;
    else if (text == "wrist") orientation = ControllerOrientation::Wrist;
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
    const bool legacy = values["version"] == "1";
    if (!legacy && values["version"] != "2") {
        error = "Unsupported overlay settings version.";
        return false;
    }

    OverlaySettings loaded;
    if (!ParseMode(values["mode"], loaded.placementMode) ||
        !ParseFloat(values["width"], 0.1F, 5.0F, loaded.widthMeters) ||
        !ParseFloat(values["distance"], 0.2F, 10.0F, loaded.distanceMeters) ||
        !ParseFloat(values["alpha"], 0.0F, 1.0F, loaded.alpha) ||
        !ParseFloat(values["offset_x"], -3.0F, 3.0F, loaded.offsetXMeters) ||
        !ParseFloat(values["offset_y"], -3.0F, 3.0F, loaded.offsetYMeters)) {
        error = "Overlay settings contain an invalid value.";
        return false;
    }
    if (legacy) {
        float distance = 0.0F;
        if (!ParseFloat(values["controller_distance"], 0.05F, 2.0F, distance)) {
            error = "Overlay settings contain an invalid controller distance.";
            return false;
        }
        loaded.leftController.distanceMeters = distance;
        loaded.rightController.distanceMeters = distance;
    } else {
        const auto parseController = [&](const char* prefix, ControllerCalibration& calibration) {
            const std::string key(prefix);
            return ParseFloat(values[key + "_distance"], 0.05F, 2.0F, calibration.distanceMeters) &&
                ParseFloat(values[key + "_height"], -1.0F, 1.0F, calibration.heightMeters) &&
                ParseFloat(values[key + "_lateral"], -1.0F, 1.0F, calibration.lateralMeters) &&
                ParseFloat(values[key + "_tilt"], -180.0F, 180.0F, calibration.tiltDegrees) &&
                ParseFloat(values[key + "_yaw"], -180.0F, 180.0F, calibration.yawDegrees) &&
                ParseFloat(values[key + "_scale"], 0.25F, 3.0F, calibration.scale) &&
                ParseOrientation(values[key + "_orientation"], calibration.orientation);
        };
        if (!parseController("left", loaded.leftController) ||
            !parseController("right", loaded.rightController)) {
            error = "Overlay settings contain an invalid controller calibration.";
            return false;
        }
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
           << "version=2\n"
           << "mode=" << ModeName(settings.placementMode) << '\n'
           << "width=" << settings.widthMeters << '\n'
           << "distance=" << settings.distanceMeters << '\n'
           << "alpha=" << settings.alpha << '\n'
           << "offset_x=" << settings.offsetXMeters << '\n'
           << "offset_y=" << settings.offsetYMeters << '\n';
    const auto writeController = [&](const char* prefix, const ControllerCalibration& calibration) {
        output << prefix << "_distance=" << calibration.distanceMeters << '\n'
               << prefix << "_height=" << calibration.heightMeters << '\n'
               << prefix << "_lateral=" << calibration.lateralMeters << '\n'
               << prefix << "_tilt=" << calibration.tiltDegrees << '\n'
               << prefix << "_yaw=" << calibration.yawDegrees << '\n'
               << prefix << "_scale=" << calibration.scale << '\n'
               << prefix << "_orientation=" << OrientationName(calibration.orientation) << '\n';
    };
    writeController("left", settings.leftController);
    writeController("right", settings.rightController);
    output << "world_valid=" << (settings.worldTransformValid ? 1 : 0) << '\n';
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
