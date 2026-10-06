package com.phonecastvr.sender;

/**
 * Owns a temporary device-brightness override without depending on Android APIs.
 * Recovery state is committed before every global-setting transition so a later
 * process can restore the user's brightness policy after an interrupted cast.
 */
final class ScreenBrightnessController {
    static final int MODE_MANUAL = 0;

    interface BrightnessAccess {
        boolean canWrite();
        int brightness();
        int mode();
        boolean writeBrightness(int value);
        boolean writeMode(int value);
    }

    interface RecoveryState {
        boolean active();
        int savedBrightness();
        int savedMode();
        int expectedBrightness();
        int expectedMode();
        boolean save(int savedBrightness, int savedMode,
                     int expectedBrightness, int expectedMode);
        boolean updateExpected(int expectedBrightness, int expectedMode);
        void clear();
    }

    private final BrightnessAccess access;
    private final RecoveryState recovery;
    private final int dimBrightness;

    ScreenBrightnessController(BrightnessAccess access, RecoveryState recovery,
                               int dimBrightness) {
        this.access = access;
        this.recovery = recovery;
        this.dimBrightness = Math.max(1, Math.min(255, dimBrightness));
    }

    boolean canWrite() { return access.canWrite(); }
    boolean ownsOverride() { return recovery.active(); }

    boolean dim() {
        if (!access.canWrite()) return false;
        if (!recovery.active() && !recovery.save(access.brightness(), access.mode(),
                dimBrightness, MODE_MANUAL)) return false;
        if (recovery.active() && !recovery.updateExpected(dimBrightness, MODE_MANUAL))
            return false;
        return applyExpected(dimBrightness, MODE_MANUAL);
    }

    boolean restoreTemporarily() {
        if (!recovery.active() || !access.canWrite()) return false;
        int brightness = recovery.savedBrightness();
        int mode = recovery.savedMode();
        if (!recovery.updateExpected(brightness, mode)) return false;
        // Restore the level while still in manual mode, then return the user's
        // original automatic/manual policy.
        if (!access.writeBrightness(brightness)) return false;
        return access.writeMode(mode);
    }

    boolean restoreAndRelease() {
        if (!recovery.active()) return true;
        if (!access.canWrite()) return false;

        int currentBrightness = access.brightness();
        int currentMode = access.mode();
        boolean stillExpected = currentBrightness == recovery.expectedBrightness()
                && currentMode == recovery.expectedMode();
        // If the user changed brightness while PhoneCast owned the override, keep
        // that deliberate level but still restore the pre-cast auto/manual policy.
        int brightnessToRestore = stillExpected
                ? recovery.savedBrightness() : currentBrightness;
        if (!access.writeBrightness(brightnessToRestore)) return false;
        if (!access.writeMode(recovery.savedMode())) return false;
        recovery.clear();
        return true;
    }

    boolean recoverAbandonedOverride() {
        return restoreAndRelease();
    }

    private boolean applyExpected(int brightness, int mode) {
        // Manual mode first makes the subsequent device-wide level predictable.
        if (!access.writeMode(mode)) return false;
        return access.writeBrightness(brightness);
    }
}
