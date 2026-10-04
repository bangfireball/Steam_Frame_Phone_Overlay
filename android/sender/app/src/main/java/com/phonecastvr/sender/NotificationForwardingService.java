package com.phonecastvr.sender;

import android.app.ActivityOptions;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.util.Log;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicLong;

/** Reads only notifications the user explicitly allows and forwards a privacy-filtered card. */
public final class NotificationForwardingService extends NotificationListenerService {
    static final String PREFERENCE_ENABLED = "notification_forwarding_enabled";
    static final String PREFERENCE_INCLUDE_CONTENT = "notification_include_content";
    static final String PREFERENCE_ALLOW_LIST = "notification_allow_list";
    static final String PREFERENCE_BLOCK_LIST = "notification_block_list";

    private static final String TAG = "PhoneCastNotify";
    private static final long DUPLICATE_WINDOW_MILLIS = 1500L;
    private static final AtomicLong NEXT_ACTION_TOKEN = new AtomicLong(1L);
    private static volatile NotificationForwardingService instance;
    private final Map<String, Forwarded> recent = new HashMap<>();
    private final Map<Long, PendingIntent> actions = new HashMap<>();
    private final Map<String, Long> actionTokensByKey = new HashMap<>();

    static boolean openNotification(long actionToken) {
        NotificationForwardingService service = instance;
        if (service == null || actionToken == 0L ||
                !service.getSharedPreferences(ScreenCaptureService.PREFERENCES, android.content.Context.MODE_PRIVATE)
                        .getBoolean(PREFERENCE_ENABLED, false)) return false;
        PendingIntent action;
        synchronized (service.actions) {
            action = service.actions.get(actionToken);
        }
        if (action == null) return false;
        service.getMainExecutor().execute(() -> {
            try {
                Bundle launchOptions = null;
                if (Build.VERSION.SDK_INT >= 34) {
                    ActivityOptions options = ActivityOptions.makeBasic();
                    options.setPendingIntentBackgroundActivityStartMode(
                            ActivityOptions.MODE_BACKGROUND_ACTIVITY_START_ALLOWED);
                    launchOptions = options.toBundle();
                }
                action.send(service, 0, null, null, null, null, launchOptions);
                Log.i(TAG, "Opened a forwarded notification action");
            } catch (PendingIntent.CanceledException error) {
                Log.w(TAG, "The forwarded notification action is no longer available");
                synchronized (service.actions) {
                    service.actions.remove(actionToken);
                }
            }
        });
        return true;
    }

    @Override public void onListenerConnected() {
        instance = this;
        super.onListenerConnected();
    }

    @Override public void onListenerDisconnected() {
        clearActions();
        super.onListenerDisconnected();
    }

    @Override public void onDestroy() {
        clearActions();
        super.onDestroy();
    }

    private void clearActions() {
        if (instance == this) instance = null;
        synchronized (actions) {
            actions.clear();
            actionTokensByKey.clear();
        }
    }

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

        long actionToken = registerAction(status.getKey(), notification.contentIntent);
        try {
            byte[] payload = StreamProtocol.notificationPayload(applicationName, title, body,
                    packageName, status.getPostTime(), actionToken, !includeContent);
            if (ScreenCaptureService.forwardNotification(payload, status.getPostTime())) {
                Log.i(TAG, "Forwarded a privacy-filtered notification from " + packageName +
                        (actionToken == 0L ? " without an open action" : " with an open action"));
            }
        } catch (IOException error) {
            Log.w(TAG, "Notification was too large to forward", error);
        }
    }

    @Override public void onNotificationRemoved(StatusBarNotification status) {
        if (status == null) return;
        synchronized (actions) {
            Long token = actionTokensByKey.remove(status.getKey());
            if (token != null) actions.remove(token);
        }
        recent.remove(status.getKey());
    }

    private long registerAction(String key, PendingIntent action) {
        synchronized (actions) {
            Long previous = actionTokensByKey.remove(key);
            if (previous != null) actions.remove(previous);
            if (action == null) return 0L;
            if (actions.size() >= 64) {
                actions.clear();
                actionTokensByKey.clear();
            }
            long token = NEXT_ACTION_TOKEN.getAndIncrement();
            if (token == 0L) token = NEXT_ACTION_TOKEN.getAndIncrement();
            actions.put(token, action);
            actionTokensByKey.put(key, token);
            return token;
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
