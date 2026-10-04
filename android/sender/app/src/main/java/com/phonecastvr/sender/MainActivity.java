package com.phonecastvr.sender;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.text.InputType;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.Locale;

public final class MainActivity extends Activity {
    private static final int REQUEST_CAPTURE = 1001;
    private static final int REQUEST_NOTIFICATIONS = 1002;

    private TextView statusView;
    private TextView metricsView;
    private EditText receiverHostView;
    private EditText pairCodeView;
    private Button startButton;
    private Button stopButton;
    private CheckBox remoteControlView;
    private Button accessibilityButton;
    private CheckBox notificationForwardingView;
    private CheckBox notificationContentView;
    private EditText notificationAllowListView;
    private EditText notificationBlockListView;
    private Button notificationAccessButton;
    private boolean captureAfterPermission;

    private final BroadcastReceiver statusReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            boolean running = intent.getBooleanExtra(ScreenCaptureService.EXTRA_RUNNING, false);
            String message = intent.getStringExtra(ScreenCaptureService.EXTRA_MESSAGE);
            long frames = intent.getLongExtra(ScreenCaptureService.EXTRA_FRAMES, 0L);
            long bytes = intent.getLongExtra(ScreenCaptureService.EXTRA_BYTES, 0L);
            int width = intent.getIntExtra(ScreenCaptureService.EXTRA_WIDTH, 0);
            int height = intent.getIntExtra(ScreenCaptureService.EXTRA_HEIGHT, 0);
            long dropped = intent.getLongExtra(ScreenCaptureService.EXTRA_DROPPED_FRAMES, 0L);
            long rttMicros = intent.getLongExtra(ScreenCaptureService.EXTRA_NETWORK_RTT_MICROS, -1L);
            updateState(running, message, frames, bytes, width, height, dropped, rttMicros);
        }
    };

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        buildUi();
        boolean running = getSharedPreferences(ScreenCaptureService.PREFERENCES, MODE_PRIVATE)
                .getBoolean("capture_running", false);
        updateState(running, running ? "Capture service active" : "Ready to stream", 0, 0, 0, 0, 0, -1);
    }

    @Override protected void onStart() {
        super.onStart();
        registerStatusReceiver();
        updateAccessibilityButton();
        updateNotificationAccessButton();
    }

    @SuppressLint("UnspecifiedRegisterReceiverFlag")
    private void registerStatusReceiver() {
        IntentFilter filter = new IntentFilter(ScreenCaptureService.ACTION_STATUS);
        if (Build.VERSION.SDK_INT >= 33) {
            registerReceiver(statusReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            // The flags overload was introduced in API 33; the broadcast is package-scoped.
            registerReceiver(statusReceiver, filter);
        }
    }

    @Override protected void onStop() {
        unregisterReceiver(statusReceiver);
        super.onStop();
    }

    private void buildUi() {
        int padding = dp(24);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER_HORIZONTAL);
        root.setPadding(padding, padding * 2, padding, padding);
        root.setBackgroundColor(Color.rgb(10, 16, 28));

        TextView title = new TextView(this);
        title.setText(R.string.app_name);
        title.setTextColor(Color.WHITE);
        title.setTextSize(28);
        title.setGravity(Gravity.CENTER);
        root.addView(title, matchWrap());

        TextView explanation = new TextView(this);
        explanation.setText(R.string.sprint_two_explanation);
        explanation.setTextColor(Color.rgb(190, 205, 220));
        explanation.setTextSize(16);
        explanation.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams explanationParams = matchWrap();
        explanationParams.setMargins(0, dp(16), 0, dp(24));
        root.addView(explanation, explanationParams);

        receiverHostView = new EditText(this);
        receiverHostView.setHint("Receiver IP address");
        receiverHostView.setSingleLine(true);
        receiverHostView.setInputType(InputType.TYPE_CLASS_PHONE);
        receiverHostView.setText(getPreferences(MODE_PRIVATE)
                .getString("receiver_host", "192.168.1.100"));
        receiverHostView.setTextColor(Color.WHITE);
        receiverHostView.setHintTextColor(Color.rgb(130, 145, 160));
        root.addView(receiverHostView, matchWrap());

        pairCodeView = new EditText(this);
        pairCodeView.setHint("6-digit pairing code");
        pairCodeView.setSingleLine(true);
        pairCodeView.setInputType(InputType.TYPE_CLASS_NUMBER);
        pairCodeView.setTextColor(Color.WHITE);
        pairCodeView.setHintTextColor(Color.rgb(130, 145, 160));
        root.addView(pairCodeView, matchWrap());

        android.content.SharedPreferences senderPreferences = getSharedPreferences(
                ScreenCaptureService.PREFERENCES, MODE_PRIVATE);
        TextView notificationDisclosure = new TextView(this);
        notificationDisclosure.setText(R.string.notification_forwarding_disclosure);
        notificationDisclosure.setTextColor(Color.rgb(190, 205, 220));
        notificationDisclosure.setTextSize(14);
        LinearLayout.LayoutParams notificationDisclosureParams = matchWrap();
        notificationDisclosureParams.setMargins(0, dp(16), 0, dp(4));
        root.addView(notificationDisclosure, notificationDisclosureParams);

        notificationForwardingView = new CheckBox(this);
        notificationForwardingView.setText(R.string.enable_notification_forwarding);
        notificationForwardingView.setTextColor(Color.WHITE);
        notificationForwardingView.setChecked(senderPreferences.getBoolean(
                NotificationForwardingService.PREFERENCE_ENABLED, false));
        root.addView(notificationForwardingView, matchWrap());

        notificationContentView = new CheckBox(this);
        notificationContentView.setText(R.string.include_notification_content);
        notificationContentView.setTextColor(Color.WHITE);
        notificationContentView.setChecked(senderPreferences.getBoolean(
                NotificationForwardingService.PREFERENCE_INCLUDE_CONTENT, false));
        root.addView(notificationContentView, matchWrap());

        notificationAllowListView = notificationListField(
                "Allowed package names (blank = all)",
                senderPreferences.getString(NotificationForwardingService.PREFERENCE_ALLOW_LIST, ""));
        root.addView(notificationAllowListView, matchWrap());
        notificationBlockListView = notificationListField(
                "Blocked package names",
                senderPreferences.getString(NotificationForwardingService.PREFERENCE_BLOCK_LIST, ""));
        root.addView(notificationBlockListView, matchWrap());

        notificationAccessButton = new Button(this);
        notificationAccessButton.setOnClickListener(view -> {
            saveNotificationPreferences();
            startActivity(new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS));
        });
        root.addView(notificationAccessButton, matchWrap());
        notificationForwardingView.setOnCheckedChangeListener((button, checked) -> {
            saveNotificationPreferences();
            ScreenCaptureService.notificationPreferencesChanged();
        });
        notificationContentView.setOnCheckedChangeListener((button, checked) -> {
            saveNotificationPreferences();
            ScreenCaptureService.notificationPreferencesChanged();
        });

        TextView remoteDisclosure = new TextView(this);
        remoteDisclosure.setText(R.string.remote_control_disclosure);
        remoteDisclosure.setTextColor(Color.rgb(190, 205, 220));
        remoteDisclosure.setTextSize(14);
        LinearLayout.LayoutParams disclosureParams = matchWrap();
        disclosureParams.setMargins(0, dp(16), 0, dp(4));
        root.addView(remoteDisclosure, disclosureParams);

        remoteControlView = new CheckBox(this);
        remoteControlView.setText(R.string.enable_remote_control);
        remoteControlView.setTextColor(Color.WHITE);
        remoteControlView.setChecked(getSharedPreferences(
                ScreenCaptureService.PREFERENCES, MODE_PRIVATE)
                .getBoolean(RemoteControlAccessibilityService.PREFERENCE_REMOTE_CONTROL, false));
        remoteControlView.setOnCheckedChangeListener((button, checked) ->
                getSharedPreferences(ScreenCaptureService.PREFERENCES, MODE_PRIVATE).edit()
                        .putBoolean(RemoteControlAccessibilityService.PREFERENCE_REMOTE_CONTROL,
                                checked).apply());
        root.addView(remoteControlView, matchWrap());

        accessibilityButton = new Button(this);
        accessibilityButton.setOnClickListener(view ->
                startActivity(new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS)));
        root.addView(accessibilityButton, matchWrap());

        statusView = new TextView(this);
        statusView.setTextColor(Color.rgb(42, 188, 251));
        statusView.setTextSize(18);
        statusView.setGravity(Gravity.CENTER);
        root.addView(statusView, matchWrap());

        metricsView = new TextView(this);
        metricsView.setTextColor(Color.WHITE);
        metricsView.setTextSize(15);
        metricsView.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams metricsParams = matchWrap();
        metricsParams.setMargins(0, dp(12), 0, dp(24));
        root.addView(metricsView, metricsParams);

        startButton = new Button(this);
        startButton.setText(R.string.start_capture);
        startButton.setOnClickListener(view -> requestCapture());
        root.addView(startButton, matchWrap());

        stopButton = new Button(this);
        stopButton.setText(R.string.stop_capture);
        stopButton.setOnClickListener(view -> stopCapture());
        LinearLayout.LayoutParams stopParams = matchWrap();
        stopParams.setMargins(0, dp(12), 0, 0);
        root.addView(stopButton, stopParams);

        ScrollView scroll = new ScrollView(this);
        scroll.addView(root);
        setContentView(scroll);
    }

    private void requestCapture() {
        saveNotificationPreferences();
        String receiverHost = receiverHostView.getText().toString().trim();
        String pairCode = pairCodeView.getText().toString().trim();
        if (receiverHost.isEmpty() || !StreamProtocol.validPairCode(pairCode)) {
            updateState(false, "Enter a receiver IP and 6-digit pairing code", 0, 0, 0, 0, 0, -1);
            return;
        }
        if (Build.VERSION.SDK_INT >= 33 &&
                checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            captureAfterPermission = true;
            requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQUEST_NOTIFICATIONS);
            return;
        }
        launchCaptureConsent();
    }

    private void launchCaptureConsent() {
        MediaProjectionManager manager =
                (MediaProjectionManager) getSystemService(MEDIA_PROJECTION_SERVICE);
        startActivityForResult(manager.createScreenCaptureIntent(), REQUEST_CAPTURE);
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions,
                                                      int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_NOTIFICATIONS && captureAfterPermission) {
            captureAfterPermission = false;
            launchCaptureConsent();
        }
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQUEST_CAPTURE) return;
        if (resultCode != RESULT_OK || data == null) {
            updateState(false, "Screen-capture permission was not granted", 0, 0, 0, 0, 0, -1);
            return;
        }
        String receiverHost = receiverHostView.getText().toString().trim();
        String pairCode = pairCodeView.getText().toString().trim();
        if (receiverHost.isEmpty() || !StreamProtocol.validPairCode(pairCode)) {
            updateState(false, "Enter a receiver IP and 6-digit pairing code", 0, 0, 0, 0, 0, -1);
            return;
        }
        getPreferences(MODE_PRIVATE).edit().putString("receiver_host", receiverHost).apply();
        Intent service = new Intent(this, ScreenCaptureService.class)
                .setAction(ScreenCaptureService.ACTION_START)
                .putExtra(ScreenCaptureService.EXTRA_RESULT_CODE, resultCode)
                .putExtra(ScreenCaptureService.EXTRA_RESULT_DATA, data)
                .putExtra(ScreenCaptureService.EXTRA_RECEIVER_HOST, receiverHost)
                .putExtra(ScreenCaptureService.EXTRA_PAIR_CODE, pairCode);
        startForegroundService(service);
        updateState(true, "Starting capture and connection…", 0, 0, 0, 0, 0, -1);
    }

    private void stopCapture() {
        Intent service = new Intent(this, ScreenCaptureService.class)
                .setAction(ScreenCaptureService.ACTION_STOP);
        startService(service);
    }

    private void updateState(boolean running, String message, long frames, long bytes,
                             int width, int height, long droppedFrames, long rttMicros) {
        getSharedPreferences(ScreenCaptureService.PREFERENCES, MODE_PRIVATE).edit()
                .putBoolean("capture_running", running).apply();
        statusView.setText(message == null ? (running ? "Capturing" : "Stopped") : message);
        if (running && width > 0) {
            metricsView.setText(String.format(Locale.US,
                    "%d × %d · %,d frames · %.1f MB · %,d dropped · %s",
                    width, height, frames, bytes / 1_000_000.0, droppedFrames,
                    rttMicros >= 0 ? String.format(Locale.US, "%.1f ms RTT", rttMicros / 1000.0)
                            : "RTT pending"));
        } else {
            metricsView.setText(running ? "Initializing H.264 encoder…" : "No active capture");
        }
        startButton.setEnabled(!running);
        stopButton.setEnabled(running);
        receiverHostView.setEnabled(!running);
        pairCodeView.setEnabled(!running);
    }

    private void updateAccessibilityButton() {
        if (accessibilityButton != null) {
            accessibilityButton.setText(RemoteControlAccessibilityService.isConnected()
                    ? R.string.remote_control_service_enabled
                    : R.string.open_accessibility_settings);
        }
    }

    private EditText notificationListField(String hint, String value) {
        EditText field = new EditText(this);
        field.setHint(hint);
        field.setSingleLine(true);
        field.setInputType(InputType.TYPE_CLASS_TEXT);
        field.setText(value);
        field.setTextColor(Color.WHITE);
        field.setHintTextColor(Color.rgb(130, 145, 160));
        field.setOnFocusChangeListener((view, focused) -> {
            if (!focused) {
                saveNotificationPreferences();
                ScreenCaptureService.notificationPreferencesChanged();
            }
        });
        return field;
    }

    private void saveNotificationPreferences() {
        if (notificationForwardingView == null) return;
        getSharedPreferences(ScreenCaptureService.PREFERENCES, MODE_PRIVATE).edit()
                .putBoolean(NotificationForwardingService.PREFERENCE_ENABLED,
                        notificationForwardingView.isChecked())
                .putBoolean(NotificationForwardingService.PREFERENCE_INCLUDE_CONTENT,
                        notificationContentView.isChecked())
                .putString(NotificationForwardingService.PREFERENCE_ALLOW_LIST,
                        notificationAllowListView.getText().toString().trim())
                .putString(NotificationForwardingService.PREFERENCE_BLOCK_LIST,
                        notificationBlockListView.getText().toString().trim())
                .apply();
    }

    private void updateNotificationAccessButton() {
        if (notificationAccessButton == null) return;
        String enabled = Settings.Secure.getString(getContentResolver(),
                "enabled_notification_listeners");
        boolean active = enabled != null && enabled.contains(getPackageName());
        notificationAccessButton.setText(active
                ? R.string.notification_access_enabled
                : R.string.open_notification_access_settings);
    }

    private LinearLayout.LayoutParams matchWrap() {
        return new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}
