package com.phonecastvr.sender;

import android.app.Application;

public final class PhoneCastApplication extends Application {
    @Override public void onCreate() {
        super.onCreate();
        // If the prior process died after muting global media volume, repair it
        // before any new activity or capture service starts.
        AndroidPhoneAudioMute.recoverAfterUncleanShutdown(this);
    }
}
