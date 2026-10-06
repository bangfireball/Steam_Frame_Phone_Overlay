package com.phonecastvr.sender;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import java.util.Locale;

public final class MainActivity extends Activity {
    static final String ACTION_QUICK_CONNECT = "com.phonecastvr.sender.action.QUICK_CONNECT";
    private static final int REQUEST_CAPTURE = 1001;
    private static final int REQUEST_NOTIFICATIONS = 1002;
    private static final int REQUEST_AUDIO = 1003;
    private static final int COLOR_BACKGROUND = Color.rgb(7, 13, 23);
    private static final int COLOR_SURFACE = Color.rgb(17, 28, 43);
    private static final int COLOR_SURFACE_HIGH = Color.rgb(24, 40, 59);
    private static final int COLOR_PRIMARY = Color.rgb(42, 188, 251);
    private static final int COLOR_TEXT = Color.rgb(245, 249, 255);
    private static final int COLOR_SECONDARY = Color.rgb(167, 187, 205);
    private static final int COLOR_SUCCESS = Color.rgb(76, 210, 145);

    private TextView screenTitleView;
    private TextView statusView;
    private TextView metricsView;
    private TextView audioStatusView;
    private CheckBox playbackAudioView;
    private CheckBox mutePhoneAudioView;
    private TextView connectionHintView;
    private LinearLayout mainContent;
    private LinearLayout settingsContent;
    private EditText receiverHostView;
    private EditText pairCodeView;
    private Button primaryButton;
    private Button menuButton;
    private Spinner streamProfileView;
    private CheckBox remoteControlView;
    private Button accessibilityButton;
    private CheckBox notificationForwardingView;
    private CheckBox notificationContentView;
    private EditText notificationAllowListView;
    private EditText notificationBlockListView;
    private Button notificationAccessButton;
    private boolean captureAfterPermission;
    private boolean receiverRegistered;
    private boolean running;

    private final BroadcastReceiver statusReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            updateState(intent.getBooleanExtra(ScreenCaptureService.EXTRA_RUNNING, false),
                    intent.getStringExtra(ScreenCaptureService.EXTRA_MESSAGE),
                    intent.getLongExtra(ScreenCaptureService.EXTRA_FRAMES, 0L),
                    intent.getLongExtra(ScreenCaptureService.EXTRA_BYTES, 0L),
                    intent.getIntExtra(ScreenCaptureService.EXTRA_WIDTH, 0),
                    intent.getIntExtra(ScreenCaptureService.EXTRA_HEIGHT, 0),
                    intent.getLongExtra(ScreenCaptureService.EXTRA_DROPPED_FRAMES, 0L),
                    intent.getLongExtra(ScreenCaptureService.EXTRA_NETWORK_RTT_MICROS, -1L));
            audioStatusView.setText(intent.getStringExtra(ScreenCaptureService.EXTRA_AUDIO_STATUS));
        }
    };

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        buildUi();
        refreshServiceState();
        handleQuickConnectIntent(getIntent());
    }

    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        refreshServiceState();
        handleQuickConnectIntent(intent);
    }

    @Override protected void onStart() {
        super.onStart();
        registerStatusReceiver();
        refreshServiceState();
        updateAccessibilityButton();
        updateNotificationAccessButton();
    }

    @Override protected void onResume() {
        super.onResume();
        refreshServiceState();
        updateAccessibilityButton();
        updateNotificationAccessButton();
        ScreenCaptureService.remoteControlStatusChanged(this);
        if (playbackAudioView != null && checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED)
            playbackAudioView.setChecked(false);
        if (audioStatusView != null) audioStatusView.setText(senderPreferences().getString(
                ScreenCaptureService.EXTRA_AUDIO_STATUS,"Audio off"));
    }

    @SuppressLint("UnspecifiedRegisterReceiverFlag")
    private void registerStatusReceiver() {
        if (receiverRegistered) return;
        IntentFilter filter = new IntentFilter(ScreenCaptureService.ACTION_STATUS);
        if (Build.VERSION.SDK_INT >= 33) {
            registerReceiver(statusReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            registerReceiver(statusReceiver, filter);
        }
        receiverRegistered = true;
    }

    @Override protected void onStop() {
        saveNotificationPreferences();
        if (receiverRegistered) {
            unregisterReceiver(statusReceiver);
            receiverRegistered = false;
        }
        super.onStop();
    }

    private void buildUi() {
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setBackgroundColor(COLOR_BACKGROUND);

        LinearLayout toolbar = new LinearLayout(this);
        toolbar.setGravity(Gravity.CENTER_VERTICAL);
        toolbar.setPadding(dp(20), dp(10), dp(12), dp(10));
        toolbar.setBackgroundColor(Color.rgb(11, 21, 34));
        screenTitleView = text("PhoneCast", 22, COLOR_TEXT, Gravity.START);
        toolbar.addView(screenTitleView, new LinearLayout.LayoutParams(0, dp(52), 1));
        menuButton = new Button(this);
        menuButton.setText("☰");
        menuButton.setTextSize(24);
        menuButton.setTextColor(COLOR_TEXT);
        menuButton.setBackgroundTintList(ColorStateList.valueOf(COLOR_SURFACE_HIGH));
        menuButton.setContentDescription("Open settings");
        menuButton.setOnClickListener(view -> showSettings(settingsContent.getVisibility() != View.VISIBLE));
        toolbar.addView(menuButton, new LinearLayout.LayoutParams(dp(56), dp(48)));
        page.addView(toolbar, matchWrap());

        LinearLayout body = new LinearLayout(this);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(dp(20), dp(20), dp(20), dp(32));
        mainContent = new LinearLayout(this);
        mainContent.setOrientation(LinearLayout.VERTICAL);
        settingsContent = new LinearLayout(this);
        settingsContent.setOrientation(LinearLayout.VERTICAL);
        settingsContent.setVisibility(View.GONE);
        body.addView(mainContent, matchWrap());
        body.addView(settingsContent, matchWrap());
        buildMainContent();
        buildSettingsContent();

        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.addView(body);
        page.addView(scroll, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));
        setContentView(page);
    }

    private void buildMainContent() {
        TextView eyebrow = text("YOUR PHONE IN VR", 13, COLOR_PRIMARY, Gravity.CENTER);
        eyebrow.setLetterSpacing(0.16f);
        mainContent.addView(eyebrow, matchWrap());
        TextView heading = text("Ready when you are", 30, COLOR_TEXT, Gravity.CENTER);
        LinearLayout.LayoutParams headingParams = matchWrap();
        headingParams.setMargins(0, dp(8), 0, dp(8));
        mainContent.addView(heading, headingParams);
        TextView intro = text("Connect to your trusted receiver, approve screen sharing, and keep playing.",
                16, COLOR_SECONDARY, Gravity.CENTER);
        LinearLayout.LayoutParams introParams = matchWrap();
        introParams.setMargins(dp(8), 0, dp(8), dp(24));
        mainContent.addView(intro, introParams);

        LinearLayout statusCard = card();
        statusView = text("Ready to cast", 20, COLOR_PRIMARY, Gravity.START);
        statusCard.addView(statusView, matchWrap());
        metricsView = text("No active capture", 14, COLOR_SECONDARY, Gravity.START);
        LinearLayout.LayoutParams metricsParams = matchWrap();
        metricsParams.setMargins(0, dp(8), 0, 0);
        statusCard.addView(metricsView, metricsParams);
        audioStatusView = text("Audio off",13,COLOR_SECONDARY,Gravity.START);
        statusCard.addView(audioStatusView,spaced(0,dp(8),0,0));
        mainContent.addView(statusCard, cardParams());

        LinearLayout connectionCard = card();
        connectionCard.addView(sectionTitle("Receiver"), matchWrap());
        connectionHintView = text("Saved details make the next cast a two-step start.",
                14, COLOR_SECONDARY, Gravity.START);
        LinearLayout.LayoutParams hintParams = matchWrap();
        hintParams.setMargins(0, dp(4), 0, dp(14));
        connectionCard.addView(connectionHintView, hintParams);

        SharedPreferences preferences = senderPreferences();
        String legacyHost = getPreferences(MODE_PRIVATE)
                .getString("receiver_host", "192.168.1.100");
        receiverHostView = input("Receiver IP address",
                preferences.getString("receiver_host", legacyHost),
                InputType.TYPE_CLASS_PHONE);
        connectionCard.addView(receiverHostView, fieldParams());
        pairCodeView = input("6-digit pairing code",
                preferences.getString("pair_code", ""),
                InputType.TYPE_CLASS_NUMBER);
        connectionCard.addView(pairCodeView, fieldParams());
        mainContent.addView(connectionCard, cardParams());

        primaryButton = new Button(this);
        primaryButton.setText(R.string.start_capture);
        primaryButton.setTextSize(18);
        primaryButton.setTextColor(Color.rgb(3, 18, 28));
        primaryButton.setAllCaps(false);
        primaryButton.setBackgroundTintList(ColorStateList.valueOf(COLOR_PRIMARY));
        primaryButton.setOnClickListener(view -> {
            if (running) stopCapture(); else requestCapture();
        });
        LinearLayout.LayoutParams actionParams = matchWrap();
        actionParams.height = dp(58);
        actionParams.setMargins(0, dp(8), 0, 0);
        mainContent.addView(primaryButton, actionParams);

        TextView privacy = text("Screen and control data stay on your local network. The current development transport is not encrypted.",
                12, COLOR_SECONDARY, Gravity.CENTER);
        LinearLayout.LayoutParams privacyParams = matchWrap();
        privacyParams.setMargins(dp(12), dp(14), dp(12), 0);
        mainContent.addView(privacy, privacyParams);
    }

    private void buildSettingsContent() {
        TextView heading = text("Settings", 30, COLOR_TEXT, Gravity.START);
        settingsContent.addView(heading, matchWrap());
        TextView intro = text("Optional features and privacy controls", 15,
                COLOR_SECONDARY, Gravity.START);
        LinearLayout.LayoutParams introParams = matchWrap();
        introParams.setMargins(0, dp(4), 0, dp(16));
        settingsContent.addView(intro, introParams);

        SharedPreferences preferences = senderPreferences();

        LinearLayout performanceCard = card();
        performanceCard.addView(sectionTitle("Streaming quality"), matchWrap());
        performanceCard.addView(text(
                "Battery Saver reduces resolution and frame rate. Standard keeps the validated baseline. Quality increases resolution and may affect the VR game.",
                13, COLOR_SECONDARY, Gravity.START), spaced(0, dp(6), 0, dp(10)));
        streamProfileView = new Spinner(this);
        ArrayAdapter<CaptureConfig.StreamProfile> profileAdapter = new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_item, CaptureConfig.StreamProfile.values());
        profileAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        streamProfileView.setAdapter(profileAdapter);
        CaptureConfig.StreamProfile storedProfile = CaptureConfig.StreamProfile.fromStoredName(
                preferences.getString("stream_profile", CaptureConfig.StreamProfile.STANDARD.name()));
        streamProfileView.setSelection(storedProfile.ordinal());
        streamProfileView.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(android.widget.AdapterView<?> parent, View view,
                                                  int position, long id) {
                CaptureConfig.StreamProfile selected =
                        (CaptureConfig.StreamProfile) parent.getItemAtPosition(position);
                senderPreferences().edit().putString("stream_profile", selected.name()).apply();
            }

            @Override public void onNothingSelected(android.widget.AdapterView<?> parent) {}
        });
        performanceCard.addView(streamProfileView, fieldParams());
        settingsContent.addView(performanceCard, cardParams());

        LinearLayout audioCard = card();
        audioCard.addView(sectionTitle("Phone playback audio"),matchWrap());
        audioCard.addView(text("Optional playback-only capture; never microphone or calls. Android asks for audio recording permission. Some apps block capture or return silence. Audio is copied, so your phone may still play sound unless the separate mute option is enabled. Use only a trusted LAN. Enabling capture applies to the next cast; disabling stops audio immediately.",
                13,COLOR_SECONDARY,Gravity.START),spaced(0,dp(6),0,dp(10)));
        playbackAudioView = new CheckBox(this);
        playbackAudioView.setText("Stream phone playback audio");
        playbackAudioView.setTextColor(COLOR_TEXT);
        playbackAudioView.setChecked(preferences.getBoolean(PlaybackAudioCapture.PREFERENCE_ENABLED,false) &&
                checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED);
        playbackAudioView.setOnCheckedChangeListener((button,checked) -> {
            if (checked && checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
                playbackAudioView.setChecked(false);
                requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO},REQUEST_AUDIO);
                return;
            }
            preferences.edit().putBoolean(PlaybackAudioCapture.PREFERENCE_ENABLED,checked).apply();
            ScreenCaptureService.audioPreferencesChanged();
            mutePhoneAudioView.setEnabled(checked);
            audioStatusView.setText(checked ? "Playback audio enabled for next cast" : "Audio off");
        });
        audioCard.addView(playbackAudioView,matchWrap());
        mutePhoneAudioView = new CheckBox(this);
        mutePhoneAudioView.setText("Mute phone while streaming audio");
        mutePhoneAudioView.setTextColor(COLOR_TEXT);
        mutePhoneAudioView.setChecked(preferences.getBoolean(
                AndroidPhoneAudioMute.PREFERENCE_ENABLED,false));
        mutePhoneAudioView.setEnabled(playbackAudioView.isChecked());
        mutePhoneAudioView.setOnCheckedChangeListener((button,checked) -> {
            preferences.edit().putBoolean(AndroidPhoneAudioMute.PREFERENCE_ENABLED,checked).apply();
            ScreenCaptureService.audioMutePreferencesChanged();
        });
        audioCard.addView(mutePhoneAudioView,matchWrap());
        audioCard.addView(text("When enabled, PhoneCast saves the current media volume, mutes the phone only while headset audio is actively streamed, and restores that volume when audio disconnects or casting stops. An unclean process stop is repaired the next time PhoneCast starts.",
                12,COLOR_SECONDARY,Gravity.START),spaced(dp(4),dp(2),0,0));
        settingsContent.addView(audioCard,cardParams());

        LinearLayout remoteCard = card();
        remoteCard.addView(sectionTitle("VR remote control"), matchWrap());
        remoteCard.addView(text(getString(R.string.remote_control_disclosure), 13,
                COLOR_SECONDARY, Gravity.START), spaced(0, dp(6), 0, dp(10)));
        remoteControlView = new CheckBox(this);
        remoteControlView.setText(R.string.enable_remote_control);
        remoteControlView.setTextColor(COLOR_TEXT);
        remoteControlView.setButtonTintList(new ColorStateList(
                new int[][]{new int[]{android.R.attr.state_checked}, new int[]{}},
                new int[]{COLOR_PRIMARY, COLOR_SECONDARY}));
        remoteControlView.setChecked(preferences.getBoolean(
                RemoteControlAccessibilityService.PREFERENCE_REMOTE_CONTROL, false));
        remoteControlView.setOnCheckedChangeListener((button, checked) -> {
            preferences.edit().putBoolean(
                    RemoteControlAccessibilityService.PREFERENCE_REMOTE_CONTROL, checked).apply();
            ScreenCaptureService.remoteControlStatusChanged(this);
        });
        remoteCard.addView(remoteControlView, matchWrap());
        accessibilityButton = secondaryButton();
        accessibilityButton.setOnClickListener(view ->
                startActivity(new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS)));
        remoteCard.addView(accessibilityButton, buttonParams());
        settingsContent.addView(remoteCard, cardParams());

        LinearLayout notificationCard = card();
        notificationCard.addView(sectionTitle("Notification cards"), matchWrap());
        notificationCard.addView(text(getString(R.string.notification_forwarding_disclosure), 13,
                COLOR_SECONDARY, Gravity.START), spaced(0, dp(6), 0, dp(10)));
        notificationForwardingView = checkBox(R.string.enable_notification_forwarding,
                preferences.getBoolean(NotificationForwardingService.PREFERENCE_ENABLED, false));
        notificationContentView = checkBox(R.string.include_notification_content,
                preferences.getBoolean(NotificationForwardingService.PREFERENCE_INCLUDE_CONTENT, false));
        notificationCard.addView(notificationForwardingView, matchWrap());
        notificationCard.addView(notificationContentView, matchWrap());
        notificationAllowListView = input("Allowed package names (blank = all)",
                preferences.getString(NotificationForwardingService.PREFERENCE_ALLOW_LIST, ""),
                InputType.TYPE_CLASS_TEXT);
        notificationBlockListView = input("Blocked package names",
                preferences.getString(NotificationForwardingService.PREFERENCE_BLOCK_LIST, ""),
                InputType.TYPE_CLASS_TEXT);
        notificationCard.addView(notificationAllowListView, fieldParams());
        notificationCard.addView(notificationBlockListView, fieldParams());
        notificationAccessButton = secondaryButton();
        notificationAccessButton.setOnClickListener(view -> {
            saveNotificationPreferences();
            startActivity(new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS));
        });
        notificationCard.addView(notificationAccessButton, buttonParams());
        notificationForwardingView.setOnCheckedChangeListener((button, checked) -> {
            saveNotificationPreferences();
            ScreenCaptureService.notificationPreferencesChanged();
        });
        notificationContentView.setOnCheckedChangeListener((button, checked) -> {
            saveNotificationPreferences();
            ScreenCaptureService.notificationPreferencesChanged();
        });
        settingsContent.addView(notificationCard, cardParams());

        Button done = secondaryButton();
        done.setText("Done");
        done.setOnClickListener(view -> showSettings(false));
        settingsContent.addView(done, buttonParams());
    }

    private void showSettings(boolean show) {
        saveNotificationPreferences();
        mainContent.setVisibility(show ? View.GONE : View.VISIBLE);
        settingsContent.setVisibility(show ? View.VISIBLE : View.GONE);
        screenTitleView.setText(show ? "PhoneCast settings" : "PhoneCast");
        menuButton.setText(show ? "×" : "☰");
        menuButton.setContentDescription(show ? "Close settings" : "Open settings");
        if (show) {
            updateAccessibilityButton();
            updateNotificationAccessButton();
        }
    }

    private void refreshServiceState() {
        boolean active = senderPreferences().getBoolean("capture_running", false);
        if (active != running) {
            updateState(active, active ? "Casting is active" : "Ready to cast",
                    0, 0, 0, 0, 0, -1);
        } else if (!active) {
            updateState(false, "Ready to cast", 0, 0, 0, 0, 0, -1);
        }
    }

    private void handleQuickConnectIntent(Intent intent) {
        if (intent == null || !ACTION_QUICK_CONNECT.equals(intent.getAction())) return;
        intent.setAction(null);
        if (running) {
            updateState(true, "Casting is already active", 0, 0, 0, 0, 0, -1);
            return;
        }
        SharedPreferences preferences = senderPreferences();
        String receiverHost = preferences.getString("receiver_host", "");
        String pairCode = preferences.getString("pair_code", "");
        if (!QuickConnectConfig.isReady(receiverHost, pairCode)) {
            updateState(false, "Open PhoneCast and save a receiver address and pairing code first",
                    0, 0, 0, 0, 0, -1);
            return;
        }
        receiverHostView.setText(receiverHost);
        pairCodeView.setText(pairCode);
        requestCapture();
    }

    private void requestCapture() {
        saveNotificationPreferences();
        String receiverHost = receiverHostView.getText().toString().trim();
        String pairCode = pairCodeView.getText().toString().trim();
        if (receiverHost.isEmpty() || !StreamProtocol.validPairCode(pairCode)) {
            updateState(false, "Check the receiver IP and 6-digit code", 0, 0, 0, 0, 0, -1);
            return;
        }
        CaptureConfig.StreamProfile profile = selectedStreamProfile();
        senderPreferences().edit()
                .putString("receiver_host", receiverHost)
                .putString("pair_code", pairCode)
                .putString("stream_profile", profile.name())
                .apply();
        if (Build.VERSION.SDK_INT >= 33 &&
                checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) !=
                        PackageManager.PERMISSION_GRANTED) {
            captureAfterPermission = true;
            requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS},
                    REQUEST_NOTIFICATIONS);
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
        if (requestCode == REQUEST_AUDIO) {
            boolean granted = grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED;
            playbackAudioView.setChecked(granted);
            if (!granted) audioStatusView.setText("Audio permission denied; video remains available");
        }
        if (requestCode == REQUEST_NOTIFICATIONS && captureAfterPermission) {
            captureAfterPermission = false;
            launchCaptureConsent();
        }
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQUEST_CAPTURE) return;
        if (resultCode != RESULT_OK || data == null) {
            updateState(false, "Screen sharing was not approved", 0, 0, 0, 0, 0, -1);
            return;
        }
        String receiverHost = receiverHostView.getText().toString().trim();
        String pairCode = pairCodeView.getText().toString().trim();
        if (receiverHost.isEmpty() || !StreamProtocol.validPairCode(pairCode)) {
            updateState(false, "Check the receiver IP and 6-digit code", 0, 0, 0, 0, 0, -1);
            return;
        }
        Intent service = new Intent(this, ScreenCaptureService.class)
                .setAction(ScreenCaptureService.ACTION_START)
                .putExtra(ScreenCaptureService.EXTRA_RESULT_CODE, resultCode)
                .putExtra(ScreenCaptureService.EXTRA_RESULT_DATA, data)
                .putExtra(ScreenCaptureService.EXTRA_RECEIVER_HOST, receiverHost)
                .putExtra(ScreenCaptureService.EXTRA_PAIR_CODE, pairCode)
                .putExtra(ScreenCaptureService.EXTRA_STREAM_PROFILE,
                        selectedStreamProfile().name());
        startForegroundService(service);
        updateState(true, "Starting secure screen permission session…",
                0, 0, 0, 0, 0, -1);
    }

    private void stopCapture() {
        startService(new Intent(this, ScreenCaptureService.class)
                .setAction(ScreenCaptureService.ACTION_STOP));
    }

    private void updateState(boolean isRunning, String message, long frames, long bytes,
                             int width, int height, long droppedFrames, long rttMicros) {
        running = isRunning;
        senderPreferences().edit().putBoolean("capture_running", isRunning).apply();
        statusView.setText(message == null ? (isRunning ? "Casting" : "Ready to cast") : message);
        statusView.setTextColor(isRunning ? COLOR_SUCCESS : COLOR_PRIMARY);
        if (isRunning && width > 0) {
            metricsView.setText(String.format(Locale.US,
                    "%d × %d  ·  %,d frames\n%.1f MB  ·  %,d dropped  ·  %s",
                    width, height, frames, bytes / 1_000_000.0, droppedFrames,
                    rttMicros >= 0 ? String.format(Locale.US, "%.1f ms RTT", rttMicros / 1000.0)
                            : "RTT pending"));
        } else {
            metricsView.setText(isRunning ? "Preparing video and receiver connection…"
                    : "No active capture");
        }
        primaryButton.setText(isRunning ? "Stop casting" : "Start casting");
        primaryButton.setBackgroundTintList(ColorStateList.valueOf(
                isRunning ? Color.rgb(242, 103, 112) : COLOR_PRIMARY));
        receiverHostView.setEnabled(!isRunning);
        pairCodeView.setEnabled(!isRunning);
        if (streamProfileView != null) streamProfileView.setEnabled(!isRunning);
        connectionHintView.setText(isRunning ? "Connection details are locked while casting."
                : "Saved details make the next cast a two-step start.");
    }

    private CaptureConfig.StreamProfile selectedStreamProfile() {
        if (streamProfileView != null &&
                streamProfileView.getSelectedItem() instanceof CaptureConfig.StreamProfile) {
            return (CaptureConfig.StreamProfile) streamProfileView.getSelectedItem();
        }
        return CaptureConfig.StreamProfile.fromStoredName(senderPreferences().getString(
                "stream_profile", CaptureConfig.StreamProfile.STANDARD.name()));
    }

    private void updateAccessibilityButton() {
        if (accessibilityButton != null) {
            accessibilityButton.setText(RemoteControlAccessibilityService.isConnected()
                    ? R.string.remote_control_service_enabled
                    : R.string.open_accessibility_settings);
        }
    }

    private void saveNotificationPreferences() {
        if (notificationForwardingView == null) return;
        senderPreferences().edit()
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

    private SharedPreferences senderPreferences() {
        return getSharedPreferences(ScreenCaptureService.PREFERENCES, MODE_PRIVATE);
    }

    private LinearLayout card() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(18), dp(18), dp(18), dp(18));
        GradientDrawable background = new GradientDrawable();
        background.setColor(COLOR_SURFACE);
        background.setCornerRadius(dp(18));
        background.setStroke(dp(1), Color.rgb(37, 58, 78));
        card.setBackground(background);
        return card;
    }

    private TextView sectionTitle(String value) {
        TextView title = text(value, 19, COLOR_TEXT, Gravity.START);
        title.setTypeface(title.getTypeface(), android.graphics.Typeface.BOLD);
        return title;
    }

    private TextView text(String value, int size, int color, int gravity) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(size);
        view.setTextColor(color);
        view.setGravity(gravity | Gravity.CENTER_VERTICAL);
        view.setLineSpacing(0, 1.12f);
        return view;
    }

    private EditText input(String hint, String value, int inputType) {
        EditText field = new EditText(this);
        field.setHint(hint);
        field.setSingleLine(true);
        field.setInputType(inputType);
        field.setText(value);
        field.setTextColor(COLOR_TEXT);
        field.setHintTextColor(Color.rgb(119, 143, 163));
        field.setPadding(dp(14), 0, dp(14), 0);
        GradientDrawable background = new GradientDrawable();
        background.setColor(COLOR_SURFACE_HIGH);
        background.setCornerRadius(dp(12));
        background.setStroke(dp(1), Color.rgb(48, 72, 94));
        field.setBackground(background);
        return field;
    }

    private CheckBox checkBox(int textResource, boolean checked) {
        CheckBox checkBox = new CheckBox(this);
        checkBox.setText(textResource);
        checkBox.setTextColor(COLOR_TEXT);
        checkBox.setChecked(checked);
        checkBox.setButtonTintList(new ColorStateList(
                new int[][]{new int[]{android.R.attr.state_checked}, new int[]{}},
                new int[]{COLOR_PRIMARY, COLOR_SECONDARY}));
        return checkBox;
    }

    private Button secondaryButton() {
        Button button = new Button(this);
        button.setTextColor(COLOR_TEXT);
        button.setTextSize(14);
        button.setAllCaps(false);
        button.setBackgroundTintList(ColorStateList.valueOf(COLOR_SURFACE_HIGH));
        return button;
    }

    private LinearLayout.LayoutParams matchWrap() {
        return new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private LinearLayout.LayoutParams cardParams() {
        LinearLayout.LayoutParams params = matchWrap();
        params.setMargins(0, 0, 0, dp(14));
        return params;
    }

    private LinearLayout.LayoutParams fieldParams() {
        LinearLayout.LayoutParams params = matchWrap();
        params.height = dp(54);
        params.setMargins(0, 0, 0, dp(10));
        return params;
    }

    private LinearLayout.LayoutParams buttonParams() {
        LinearLayout.LayoutParams params = matchWrap();
        params.height = dp(52);
        params.setMargins(0, dp(10), 0, 0);
        return params;
    }

    private LinearLayout.LayoutParams spaced(int left, int top, int right, int bottom) {
        LinearLayout.LayoutParams params = matchWrap();
        params.setMargins(left, top, right, bottom);
        return params;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}
