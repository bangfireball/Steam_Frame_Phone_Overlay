package com.phonecastvr.sender;

import android.app.Notification;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.util.Log;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/** Reads only notifications the user explicitly allows and forwards a privacy-filtered card. */
public final class NotificationForwardingService extends NotificationListenerService {
    static final String PREFERENCE_ENABLED = "notification_forwarding_enabled";
    static final String PREFERENCE_INCLUDE_CONTENT = "notification_include_content";
    static final String PREFERENCE_ALLOW_LIST = "notification_allow_list";
    static final String PREFERENCE_BLOCK_LIST = "notification_block_list";

    private static final String TAG = "PhoneCastNotify";
    private static final long DUPLICATE_WINDOW_MILLIS = 1500L;
    private final Map<String, Forwarded> recent = new HashMap<>();

    @Override public void onNotificationPosted(StatusBarNotification status) {
        if (status == null || status.getNotification() == null) return;
        SharedPreferences preferences = getSharedPreferences(
                ScreenCaptureService.PREFERENCES, MODE_PRIVATE);
        if (!preferences.getBoolean(PREFERENCE_ENABLED, false)) return;
        String packageName = status.getPackageName();
        if (getPackageName().equals(packageName) || !NotificationPolicy.allows(
                packageName,
                preferences.getString(PREFERENCE_ALLOW_LIST, ""),
                preferences.getString(PREFERENCE_BLOCK_LIST, ""))) return;

        Notification notification = status.getNotification();
        if ((notification.flags & Notification.FLAG_GROUP_SUMMARY) != 0) return;
        Bundle extras = notification.extras;
        boolean includeContent = preferences.getBoolean(PREFERENCE_INCLUDE_CONTENT, false);
        String applicationName = applicationName(packageName);
        String title = includeContent
                ? NotificationPolicy.clean(extras.getCharSequence(Notification.EXTRA_TITLE), 96)
                : "New notification";
        CharSequence expanded = extras.getCharSequence(Notification.EXTRA_BIG_TEXT);
        if (expanded == null) expanded = extras.getCharSequence(Notification.EXTRA_TEXT);
        String body = includeContent ? NotificationPolicy.clean(expanded, 240) : "";
        if (title.isEmpty()) title = includeContent ? "Notification" : "New notification";

        String fingerprint = applicationName + '\n' + title + '\n' + body;
        long now = System.currentTimeMillis();
        Forwarded previous = recent.get(status.getKey());
        if (previous != null && previous.fingerprint.equals(fingerprint) &&
                now - previous.timeMillis < DUPLICATE_WINDOW_MILLIS) return;
        recent.put(status.getKey(), new Forwarded(fingerprint, now));
        if (recent.size() > 64) recent.clear();

        try {
            byte[] payload = StreamProtocol.notificationPayload(applicationName, title, body,
                    packageName, status.getPostTime(), !includeContent);
            if (ScreenCaptureService.forwardNotification(payload, status.getPostTime())) {
                Log.i(TAG, "Forwarded a privacy-filtered notification from " + packageName);
            }
        } catch (IOException error) {
            Log.w(TAG, "Notification was too large to forward", error);
        }
    }

    private String applicationName(String packageName) {
        try {
            return NotificationPolicy.clean(getPackageManager()
                    .getApplicationLabel(getPackageManager().getApplicationInfo(packageName, 0)), 64);
        } catch (Exception ignored) {
            return NotificationPolicy.clean(packageName, 64);
        }
    }

    private static final class Forwarded {
        final String fingerprint;
        final long timeMillis;

        Forwarded(String fingerprint, long timeMillis) {
            this.fingerprint = fingerprint;
            this.timeMillis = timeMillis;
        }
    }
}
