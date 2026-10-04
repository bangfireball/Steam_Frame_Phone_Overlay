#include "phonecast/vr/overlay/SettingsMenuController.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace phonecast::vr {
namespace {

float Clamp(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(maximum, value));
}

float Normalize(float value, float minimum, float maximum) {
    return Clamp((value - minimum) / (maximum - minimum), 0.0F, 1.0F);
}

float FromNormalized(float value, float minimum, float maximum) {
    return minimum + Clamp(value, 0.0F, 1.0F) * (maximum - minimum);
}

std::string Decimal(float value, int precision = 2) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

const char* PlacementName(PlacementMode mode) {
    switch (mode) {
        case PlacementMode::HeadLocked: return "HEAD";
        case PlacementMode::WorldLocked: return "WORLD";
        case PlacementMode::LeftControllerLocked: return "LEFT CONTROLLER";
        case PlacementMode::RightControllerLocked: return "RIGHT CONTROLLER";
    }
    return "UNKNOWN";
}

const char* OrientationName(ControllerOrientation orientation) {
    switch (orientation) {
        case ControllerOrientation::ControllerRelative: return "CONTROLLER";
        case ControllerOrientation::FaceUser: return "FACE USER";
        case ControllerOrientation::WorldUpright: return "WORLD UPRIGHT";
        case ControllerOrientation::Wrist: return "WRIST";
    }
    return "UNKNOWN";
}

ControllerCalibration& CalibrationFor(OverlaySettings& settings, bool left) {
    return left ? settings.leftController : settings.rightController;
}

const ControllerCalibration& CalibrationFor(const OverlaySettings& settings, bool left) {
    return left ? settings.leftController : settings.rightController;
}

}  // namespace

void SettingsMenuController::Open(const OverlaySettings& settings) {
    original_ = settings;
    draft_ = settings;
    page_ = Page::Root;
    selected_ = 0;
    resetConfirmation_ = false;
    open_ = true;
}

void SettingsMenuController::MergeRendererUpdate(const OverlaySettings& settings) noexcept {
    if (!open_) return;
    draft_.placementMode = settings.placementMode;
    draft_.leftController = settings.leftController;
    draft_.rightController = settings.rightController;
    draft_.worldTransform = settings.worldTransform;
    draft_.worldTransformValid = settings.worldTransformValid;
}

std::size_t SettingsMenuController::ItemCount() const noexcept {
    switch (page_) {
        case Page::Root: return 9;
        case Page::Appearance: return 4;
        case Page::Placement: return 5;
        case Page::LeftController:
        case Page::RightController: return 9;
        case Page::Glance: return 3;
        case Page::Notifications: return 5;
    }
    return 0;
}

SettingsMenuView SettingsMenuController::View() const {
    SettingsMenuView view;
    view.selectedIndex = selected_;
    switch (page_) {
        case Page::Root:
            view.title = "PHONECAST SETTINGS";
            view.labels = {"APPEARANCE", "PLACEMENT", "LEFT DOCK", "RIGHT DOCK",
                           "GLANCE AND CONTROLS", "NOTIFICATIONS", "APPLY", "CANCEL", "RESET ALL"};
            view.values = {">", ">", ">", ">", ">", ">", "", "",
                           resetConfirmation_ ? "CONFIRM" : ""};
            view.normalizedValues.assign(view.labels.size(), -1.0F);
            break;
        case Page::Appearance:
            view.title = "APPEARANCE";
            view.labels = {"OVERLAY SIZE", "OPACITY", "DISTANCE", "BACK"};
            view.values = {Decimal(draft_.widthMeters) + " M", Decimal(draft_.alpha, 2),
                           Decimal(draft_.distanceMeters) + " M", ""};
            view.normalizedValues = {Normalize(draft_.widthMeters, 0.10F, 2.0F),
                                     Normalize(draft_.alpha, 0.10F, 1.0F),
                                     Normalize(draft_.distanceMeters, 0.20F, 3.0F), -1.0F};
            break;
        case Page::Placement:
            view.title = "PLACEMENT";
            view.labels = {"MODE", "HORIZONTAL", "VERTICAL", "RESET POSITION", "BACK"};
            view.values = {PlacementName(draft_.placementMode), Decimal(draft_.offsetXMeters) + " M",
                           Decimal(draft_.offsetYMeters) + " M", "", ""};
            view.normalizedValues = {-1.0F, Normalize(draft_.offsetXMeters, -2.0F, 2.0F),
                                     Normalize(draft_.offsetYMeters, -2.0F, 2.0F), -1.0F, -1.0F};
            break;
        case Page::LeftController:
        case Page::RightController: {
            const bool left = page_ == Page::LeftController;
            const auto& calibration = CalibrationFor(draft_, left);
            view.title = left ? "LEFT DOCK" : "RIGHT DOCK";
            view.labels = {"DISTANCE", "HEIGHT", "LATERAL", "TILT", "YAW", "SCALE",
                           "ORIENTATION", "RESET HAND", "BACK"};
            view.values = {Decimal(calibration.distanceMeters) + " M",
                           Decimal(calibration.heightMeters) + " M",
                           Decimal(calibration.lateralMeters) + " M",
                           Decimal(calibration.tiltDegrees, 0) + " DEG",
                           Decimal(calibration.yawDegrees, 0) + " DEG",
                           Decimal(calibration.scale), OrientationName(calibration.orientation), "", ""};
            view.normalizedValues = {
                Normalize(calibration.distanceMeters, 0.05F, 2.0F),
                Normalize(calibration.heightMeters, -1.0F, 1.0F),
                Normalize(calibration.lateralMeters, -1.0F, 1.0F),
                Normalize(calibration.tiltDegrees, -180.0F, 180.0F),
                Normalize(calibration.yawDegrees, -180.0F, 180.0F),
                Normalize(calibration.scale, 0.25F, 3.0F), -1.0F, -1.0F, -1.0F};
            break;
        }
        case Page::Glance:
            view.title = "GLANCE AND CONTROLS";
            view.labels = {"PREVIEW SCALE", "BUTTON HOLD", "BACK"};
            view.values = {Decimal(draft_.glancePreviewScale),
                           std::to_string(draft_.radialLongPressMilliseconds) + " MS", ""};
            view.normalizedValues = {Normalize(draft_.glancePreviewScale, 0.25F, 1.0F),
                                     Normalize(static_cast<float>(draft_.radialLongPressMilliseconds),
                                               250.0F, 1500.0F), -1.0F};
            break;
        case Page::Notifications:
            view.title = "NOTIFICATIONS";
            view.showNotificationPreview = true;
            view.labels = {"SIZE", "DISTANCE", "HORIZONTAL", "VERTICAL", "BACK"};
            view.values = {Decimal(draft_.notificationWidthMeters) + " M",
                           Decimal(draft_.notificationDistanceMeters) + " M",
                           Decimal(draft_.notificationOffsetXMeters) + " M",
                           Decimal(draft_.notificationOffsetYMeters) + " M", ""};
            view.normalizedValues = {
                Normalize(draft_.notificationWidthMeters, 0.20F, 1.20F),
                Normalize(draft_.notificationDistanceMeters, 0.40F, 2.00F),
                Normalize(draft_.notificationOffsetXMeters, -1.00F, 1.00F),
                Normalize(draft_.notificationOffsetYMeters, -0.75F, 0.75F), -1.0F};
            break;
    }
    return view;
}

SettingsMenuResult SettingsMenuController::Adjust(int direction) {
    resetConfirmation_ = false;
    switch (page_) {
        case Page::Root:
            return SettingsMenuResult::None;
        case Page::Appearance:
            if (selected_ == 0)
                draft_.widthMeters = Clamp(draft_.widthMeters + direction * 0.05F, 0.10F, 2.0F);
            else if (selected_ == 1)
                draft_.alpha = Clamp(draft_.alpha + direction * 0.05F, 0.10F, 1.0F);
            else if (selected_ == 2)
                draft_.distanceMeters = Clamp(draft_.distanceMeters + direction * 0.05F, 0.20F, 3.0F);
            else return SettingsMenuResult::None;
            return SettingsMenuResult::Updated;
        case Page::Placement:
            if (selected_ == 0) {
                int mode = static_cast<int>(draft_.placementMode);
                mode = (mode + direction + 4) % 4;
                draft_.placementMode = static_cast<PlacementMode>(mode);
            } else if (selected_ == 1) {
                draft_.offsetXMeters = Clamp(draft_.offsetXMeters + direction * 0.05F, -2.0F, 2.0F);
            } else if (selected_ == 2) {
                draft_.offsetYMeters = Clamp(draft_.offsetYMeters + direction * 0.05F, -2.0F, 2.0F);
            } else return SettingsMenuResult::None;
            return SettingsMenuResult::Updated;
        case Page::LeftController:
        case Page::RightController: {
            auto& calibration = CalibrationFor(draft_, page_ == Page::LeftController);
            if (selected_ == 0)
                calibration.distanceMeters = Clamp(calibration.distanceMeters + direction * 0.01F, 0.05F, 2.0F);
            else if (selected_ == 1)
                calibration.heightMeters = Clamp(calibration.heightMeters + direction * 0.01F, -1.0F, 1.0F);
            else if (selected_ == 2)
                calibration.lateralMeters = Clamp(calibration.lateralMeters + direction * 0.01F, -1.0F, 1.0F);
            else if (selected_ == 3)
                calibration.tiltDegrees = Clamp(calibration.tiltDegrees + direction * 5.0F, -180.0F, 180.0F);
            else if (selected_ == 4)
                calibration.yawDegrees = Clamp(calibration.yawDegrees + direction * 5.0F, -180.0F, 180.0F);
            else if (selected_ == 5)
                calibration.scale = Clamp(calibration.scale + direction * 0.05F, 0.25F, 3.0F);
            else if (selected_ == 6) {
                int orientation = static_cast<int>(calibration.orientation);
                orientation = (orientation + direction + 4) % 4;
                calibration.orientation = static_cast<ControllerOrientation>(orientation);
            } else return SettingsMenuResult::None;
            return SettingsMenuResult::Updated;
        }
        case Page::Glance:
            if (selected_ == 0)
                draft_.glancePreviewScale = Clamp(
                    draft_.glancePreviewScale + direction * 0.05F, 0.25F, 1.0F);
            else if (selected_ == 1) {
                const int value = static_cast<int>(draft_.radialLongPressMilliseconds) + direction * 50;
                draft_.radialLongPressMilliseconds = static_cast<std::uint32_t>(
                    std::max(250, std::min(1500, value)));
            } else return SettingsMenuResult::None;
            return SettingsMenuResult::Updated;
        case Page::Notifications:
            if (selected_ == 0)
                draft_.notificationWidthMeters = Clamp(
                    draft_.notificationWidthMeters + direction * 0.05F, 0.20F, 1.20F);
            else if (selected_ == 1)
                draft_.notificationDistanceMeters = Clamp(
                    draft_.notificationDistanceMeters + direction * 0.05F, 0.40F, 2.00F);
            else if (selected_ == 2)
                draft_.notificationOffsetXMeters = Clamp(
                    draft_.notificationOffsetXMeters + direction * 0.05F, -1.00F, 1.00F);
            else if (selected_ == 3)
                draft_.notificationOffsetYMeters = Clamp(
                    draft_.notificationOffsetYMeters + direction * 0.05F, -0.75F, 0.75F);
            else return SettingsMenuResult::None;
            return SettingsMenuResult::Updated;
    }
    return SettingsMenuResult::None;
}

SettingsMenuResult SettingsMenuController::SetNormalized(float value) {
    resetConfirmation_ = false;
    value = Clamp(value, 0.0F, 1.0F);
    switch (page_) {
        case Page::Root:
            return SettingsMenuResult::None;
        case Page::Appearance:
            if (selected_ == 0) draft_.widthMeters = FromNormalized(value, 0.10F, 2.0F);
            else if (selected_ == 1) draft_.alpha = FromNormalized(value, 0.10F, 1.0F);
            else if (selected_ == 2) draft_.distanceMeters = FromNormalized(value, 0.20F, 3.0F);
            else return SettingsMenuResult::None;
            break;
        case Page::Placement:
            if (selected_ == 1) draft_.offsetXMeters = FromNormalized(value, -2.0F, 2.0F);
            else if (selected_ == 2) draft_.offsetYMeters = FromNormalized(value, -2.0F, 2.0F);
            else return SettingsMenuResult::None;
            break;
        case Page::LeftController:
        case Page::RightController: {
            auto& calibration = CalibrationFor(draft_, page_ == Page::LeftController);
            if (selected_ == 0) calibration.distanceMeters = FromNormalized(value, 0.05F, 2.0F);
            else if (selected_ == 1) calibration.heightMeters = FromNormalized(value, -1.0F, 1.0F);
            else if (selected_ == 2) calibration.lateralMeters = FromNormalized(value, -1.0F, 1.0F);
            else if (selected_ == 3) calibration.tiltDegrees = FromNormalized(value, -180.0F, 180.0F);
            else if (selected_ == 4) calibration.yawDegrees = FromNormalized(value, -180.0F, 180.0F);
            else if (selected_ == 5) calibration.scale = FromNormalized(value, 0.25F, 3.0F);
            else return SettingsMenuResult::None;
            break;
        }
        case Page::Glance:
            if (selected_ == 0) draft_.glancePreviewScale = FromNormalized(value, 0.25F, 1.0F);
            else if (selected_ == 1) draft_.radialLongPressMilliseconds =
                static_cast<std::uint32_t>(std::lround(FromNormalized(value, 250.0F, 1500.0F)));
            else return SettingsMenuResult::None;
            break;
        case Page::Notifications:
            if (selected_ == 0) draft_.notificationWidthMeters = FromNormalized(value, 0.20F, 1.20F);
            else if (selected_ == 1) draft_.notificationDistanceMeters = FromNormalized(value, 0.40F, 2.00F);
            else if (selected_ == 2) draft_.notificationOffsetXMeters = FromNormalized(value, -1.00F, 1.00F);
            else if (selected_ == 3) draft_.notificationOffsetYMeters = FromNormalized(value, -0.75F, 0.75F);
            else return SettingsMenuResult::None;
            break;
    }
    return SettingsMenuResult::Updated;
}

SettingsMenuResult SettingsMenuController::Activate() {
    if (page_ == Page::Root) {
        if (selected_ <= 5) {
            page_ = static_cast<Page>(static_cast<int>(Page::Appearance) + static_cast<int>(selected_));
            selected_ = 0;
            resetConfirmation_ = false;
            return SettingsMenuResult::None;
        }
        if (selected_ == 6) {
            open_ = false;
            return SettingsMenuResult::Applied;
        }
        if (selected_ == 7) {
            open_ = false;
            return SettingsMenuResult::Cancelled;
        }
        if (!resetConfirmation_) {
            resetConfirmation_ = true;
            return SettingsMenuResult::None;
        }
        draft_ = {};
        resetConfirmation_ = false;
        return SettingsMenuResult::Updated;
    }

    const auto count = ItemCount();
    if (selected_ == count - 1) {
        page_ = Page::Root;
        selected_ = 0;
        return SettingsMenuResult::None;
    }
    if (page_ == Page::Placement && selected_ == 3) {
        draft_.offsetXMeters = 0.0F;
        draft_.offsetYMeters = 0.0F;
        draft_.distanceMeters = 1.0F;
        draft_.worldTransformValid = false;
        return SettingsMenuResult::Updated;
    }
    if ((page_ == Page::LeftController || page_ == Page::RightController) && selected_ == 7) {
        CalibrationFor(draft_, page_ == Page::LeftController) = {};
        return SettingsMenuResult::Updated;
    }
    return SettingsMenuResult::None;
}

SettingsMenuResult SettingsMenuController::Handle(SettingsMenuCommand command) {
    return Handle(SettingsMenuInput{command, 0.0F});
}

SettingsMenuResult SettingsMenuController::Handle(const SettingsMenuInput& input) {
    if (!open_) return SettingsMenuResult::None;
    switch (input.command) {
        case SettingsMenuCommand::PreviousItem:
            selected_ = selected_ == 0 ? ItemCount() - 1 : selected_ - 1;
            resetConfirmation_ = false;
            return SettingsMenuResult::None;
        case SettingsMenuCommand::NextItem:
            selected_ = (selected_ + 1) % ItemCount();
            resetConfirmation_ = false;
            return SettingsMenuResult::None;
        case SettingsMenuCommand::Decrease:
            return Adjust(-1);
        case SettingsMenuCommand::Increase:
            return Adjust(1);
        case SettingsMenuCommand::SetNormalized:
            return SetNormalized(input.normalizedValue);
        case SettingsMenuCommand::Activate:
            return Activate();
        case SettingsMenuCommand::Back:
            resetConfirmation_ = false;
            if (page_ == Page::Root) {
                open_ = false;
                return SettingsMenuResult::Cancelled;
            }
            page_ = Page::Root;
            selected_ = 0;
            return SettingsMenuResult::None;
    }
    return SettingsMenuResult::None;
}

}  // namespace phonecast::vr
