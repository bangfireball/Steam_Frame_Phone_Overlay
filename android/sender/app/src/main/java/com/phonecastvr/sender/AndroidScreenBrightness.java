package com.phonecastvr.sender;

import android.content.Context;
import android.content.SharedPreferences;
import android.provider.Settings;
import android.util.Log;

/** Android Settings.System adapter for the testable brightness lifecycle. */
final class AndroidScreenBrightness {
    static final String PREFERENCE_ENABLED = "dim_phone_while_casting";
    static final String PREFERENCE_DELAY_SECONDS = "dim_phone_delay_seconds";
    static final String PREFERENCE_PERMISSION_PROMPTED = "dim_phone_permission_prompted";
    static final boolean DEFAULT_ENABLED = true;
    static final int DEFAULT_DELAY_SECONDS = 15;
    static final int DIM_BRIGHTNESS = 1;

    private static final String RECOVERY_ACTIVE = "brightness_recovery_active";
    private static final String RECOVERY_BRIGHTNESS = "brightness_recovery_value";
    private static final String RECOVERY_MODE = "brightness_recovery_mode";
    private static final String EXPECTED_BRIGHTNESS = "brightness_expected_value";
    private static final String EXPECTED_MODE = "brightness_expected_mode";
    private static final String TAG = "PhoneCastBrightness";

    static ScreenBrightnessController create(Context context) {
        Context app = context.getApplicationContext();
        SharedPreferences preferences = app.getSharedPreferences(
                ScreenCaptureService.PREFERENCES, Context.MODE_PRIVATE);
        return new ScreenBrightnessController(new ScreenBrightnessController.BrightnessAccess() {
            @Override public boolean canWrite() {
                return Settings.System.canWrite(app);
            }

            @Override public int brightness() {
                try {
                    return Settings.System.getInt(app.getContentResolver(),
                            Settings.System.SCREEN_BRIGHTNESS);
                } catch (Settings.SettingNotFoundException error) {
                    return 128;
                }
            }

            @Override public int mode() {
                try {
                    return Settings.System.getInt(app.getContentResolver(),
                            Settings.System.SCREEN_BRIGHTNESS_MODE);
                } catch (Settings.SettingNotFoundException error) {
                    return Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL;
                }
            }

            @Override public boolean writeBrightness(int value) {
                try {
                    return Settings.System.putInt(app.getContentResolver(),
                            Settings.System.SCREEN_BRIGHTNESS,
                            Math.max(1, Math.min(255, value)));
                } catch (RuntimeException error) {
                    Log.w(TAG, "Could not change screen brightness: " +
                            error.getClass().getSimpleName());
                    return false;
                }
            }

            @Override public boolean writeMode(int value) {
                try {
                    return Settings.System.putInt(app.getContentResolver(),
                            Settings.System.SCREEN_BRIGHTNESS_MODE, value);
                } catch (RuntimeException error) {
                    Log.w(TAG, "Could not change brightness mode: " +
                            error.getClass().getSimpleName());
                    return false;
                }
            }
        }, new ScreenBrightnessController.RecoveryState() {
            @Override public boolean active() {
                return preferences.getBoolean(RECOVERY_ACTIVE, false);
            }

            @Override public int savedBrightness() {
                return preferences.getInt(RECOVERY_BRIGHTNESS, 128);
            }

            @Override public int savedMode() {
                return preferences.getInt(RECOVERY_MODE,
                        Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL);
            }

            @Override public int expectedBrightness() {
                return preferences.getInt(EXPECTED_BRIGHTNESS, DIM_BRIGHTNESS);
            }

            @Override public int expectedMode() {
                return preferences.getInt(EXPECTED_MODE,
                        Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL);
            }

            @Override public boolean save(int savedBrightness, int savedMode,
                                          int expectedBrightness, int expectedMode) {
                return preferences.edit()
                        .putInt(RECOVERY_BRIGHTNESS, savedBrightness)
                        .putInt(RECOVERY_MODE, savedMode)
                        .putInt(EXPECTED_BRIGHTNESS, expectedBrightness)
                        .putInt(EXPECTED_MODE, expectedMode)
                        .putBoolean(RECOVERY_ACTIVE, true)
                        .commit();
            }

            @Override public boolean updateExpected(int expectedBrightness,
                                                    int expectedMode) {
                return preferences.edit()
                        .putInt(EXPECTED_BRIGHTNESS, expectedBrightness)
                        .putInt(EXPECTED_MODE, expectedMode)
                        .commit();
            }

            @Override public void clear() {
                preferences.edit()
                        .remove(RECOVERY_ACTIVE)
                        .remove(RECOVERY_BRIGHTNESS)
                        .remove(RECOVERY_MODE)
                        .remove(EXPECTED_BRIGHTNESS)
                        .remove(EXPECTED_MODE)
                        .commit();
            }
        }, DIM_BRIGHTNESS);
    }

    static void recoverAfterUncleanShutdown(Context context) {
        ScreenBrightnessController controller = create(context);
        if (controller.ownsOverride() && !controller.recoverAbandonedOverride()) {
            Log.w(TAG, "Could not restore brightness left dimmed by an interrupted cast");
        }
    }

    static int delaySeconds(SharedPreferences preferences) {
        int value = preferences.getInt(PREFERENCE_DELAY_SECONDS, DEFAULT_DELAY_SECONDS);
        return Math.max(5, Math.min(120, value));
    }

    private AndroidScreenBrightness() {}
}
