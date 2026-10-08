package com.phonecastvr.sender;

import android.app.Application;

public final class PhoneCastApplication extends Application {
    @Override public void onCreate() {
        super.onCreate();
        getSharedPreferences(ScreenCaptureService.PREFERENCES, MODE_PRIVATE).edit()
                .putBoolean("capture_running", false).putBoolean("receiver_responsive", false)
                .putString("connection_message", ConnectionFeedback.CONNECTING).apply();
        DiagnosticLog.record(this, "process start " + BuildConfig.SUPPORT_BUILD);
        // If the prior process died after muting global media volume, repair it
        // before any new activity or capture service starts.
        AndroidPhoneAudioMute.recoverAfterUncleanShutdown(this);
        AndroidScreenBrightness.recoverAfterUncleanShutdown(this);
        AndroidScreenTimeout.recoverAfterUncleanShutdown(this);
    }
}
