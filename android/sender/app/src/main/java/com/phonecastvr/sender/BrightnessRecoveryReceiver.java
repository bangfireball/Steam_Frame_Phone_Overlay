package com.phonecastvr.sender;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/** Best-effort recovery of a brightness override left active across a reboot. */
public final class BrightnessRecoveryReceiver extends BroadcastReceiver {
    @Override public void onReceive(Context context, Intent intent) {
        if (Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())) {
            AndroidScreenBrightness.recoverAfterUncleanShutdown(context);
        }
    }
}
