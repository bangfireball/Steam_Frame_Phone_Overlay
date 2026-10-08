package com.phonecastvr.sender;

import android.content.Context;
import android.content.SharedPreferences;
import android.provider.Settings;
import android.util.Log;

final class AndroidScreenTimeout {
    private static final String ACTIVE = "timeout_recovery_active";
    private static final String SAVED = "timeout_recovery_value";

    static ScreenTimeoutController create(Context context) {
        Context app = context.getApplicationContext();
        SharedPreferences prefs = app.getSharedPreferences(
                ScreenCaptureService.PREFERENCES, Context.MODE_PRIVATE);
        return new ScreenTimeoutController(new ScreenTimeoutController.Access() {
            public boolean canWrite() { return Settings.System.canWrite(app); }
            public int read() {
                try {
                    return Settings.System.getInt(app.getContentResolver(), Settings.System.SCREEN_OFF_TIMEOUT);
                } catch (Settings.SettingNotFoundException | RuntimeException error) {
                    return -1;
                }
            }
            public boolean write(int value) {
                try {
                    return Settings.System.putInt(app.getContentResolver(), Settings.System.SCREEN_OFF_TIMEOUT, value);
                } catch (RuntimeException error) {
                    return false;
                }
            }
        }, new ScreenTimeoutController.Recovery() {
            public boolean active() { return prefs.getBoolean(ACTIVE, false); }
            public int saved() { return prefs.getInt(SAVED, 30000); }
            public boolean save(int value) {
                return prefs.edit().putInt(SAVED, value).putBoolean(ACTIVE, true).commit();
            }
            public boolean clear() {
                return prefs.edit().remove(ACTIVE).remove(SAVED).commit();
            }
        });
    }

    static void recoverAfterUncleanShutdown(Context context) {
        if (!create(context).restore())
            Log.w("PhoneCastCapture", "Screen timeout restoration pending: Modify system settings access may be required");
    }
    private AndroidScreenTimeout() {}
}
